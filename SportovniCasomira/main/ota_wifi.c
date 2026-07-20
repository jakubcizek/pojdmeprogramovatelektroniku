#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_log.h"

#include "proto.h"
#include "ota_wifi.h"

static const char *TAG = "ota_wifi";

static bool           s_inited     = false;
static bool           s_connecting = false;   // probíhá pokus o připojení
static bool           s_connected  = false;   // máme IP
static int            s_retries    = 0;
static httpd_handle_t s_httpd      = NULL;
static char           s_token[17]  = "";      // jednorázový token pro /ota

#define MAX_RETRIES 3

// ---- HTTP server: POST /ota ----

// CORS hlavičky: stránka běží na http://localhost, deska na jiné IP.
static void set_cors(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "X-Auth, Content-Type");
}

// Preflight (prohlížeč se před POSTem s vlastní hlavičkou ptá OPTIONS).
static esp_err_t ota_options_handler(httpd_req_t *req)
{
    set_cors(req);
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static void restart_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(1500));   // ať stihne odejít HTTP odpověď i CSM řádek
    esp_restart();
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    set_cors(req);

    // Ověření: token vydaný přes důvěryhodný kanál (UART / ověřené BLE).
    char hdr[24] = "";
    if (httpd_req_get_hdr_value_str(req, "X-Auth", hdr, sizeof(hdr)) != ESP_OK ||
        strcmp(hdr, s_token) != 0) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_sendstr(req, "bad token");
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (part == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "no ota partition");
    }

    int total = req->content_len;
    if (total <= 0 || (unsigned)total > part->size) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "bad size");
    }

    esp_ota_handle_t oh;
    if (esp_ota_begin(part, total, &oh) != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "ota_begin failed");
    }

    csm_emit("CSM OTA start size=%d\n", total);
    ESP_LOGI(TAG, "OTA do '%s', %d B", part->label, total);

    char *buf = malloc(4096);
    if (buf == NULL) {
        esp_ota_abort(oh);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "no mem");
    }

    int remaining = total;
    while (remaining > 0) {
        int n = httpd_req_recv(req, buf, (remaining < 4096) ? remaining : 4096);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (n <= 0 || esp_ota_write(oh, buf, n) != ESP_OK) {
            free(buf);
            esp_ota_abort(oh);
            csm_emit("CSM OTA fail\n");
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "write failed");
        }
        remaining -= n;
    }
    free(buf);

    // esp_ota_end ověří validitu obrazu (magic, SHA), pak přepneme boot oddíl.
    if (esp_ota_end(oh) != ESP_OK ||
        esp_ota_set_boot_partition(part) != ESP_OK) {
        csm_emit("CSM OTA fail\n");
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "invalid image");
    }

    httpd_resp_sendstr(req, "OK");
    csm_emit("CSM OTA done\n");
    xTaskCreate(restart_task, "ota_restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static void httpd_start_ota(void)
{
    if (s_httpd != NULL) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start selhalo");
        return;
    }
    static const httpd_uri_t post_uri = {
        .uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler,
    };
    static const httpd_uri_t opt_uri = {
        .uri = "/ota", .method = HTTP_OPTIONS, .handler = ota_options_handler,
    };
    httpd_register_uri_handler(s_httpd, &post_uri);
    httpd_register_uri_handler(s_httpd, &opt_uri);
}

static void httpd_stop_ota(void)
{
    if (s_httpd != NULL) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
}

// ---- Wi-Fi události ----
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_connecting) {
            if (s_retries++ < MAX_RETRIES) {
                esp_wifi_connect();
            } else {
                s_connecting = false;
                csm_emit("CSM WIFI fail\n");
            }
        } else if (s_connected) {   // nečekaný výpadek (router zmizel apod.)
            s_connected = false;
            httpd_stop_ota();
            csm_emit("CSM WIFI off\n");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        s_connecting = false;
        s_connected  = true;
        // Jednorázový token: jen ten, kdo ovládá desku přes UART/BLE, smí flashovat.
        snprintf(s_token, sizeof(s_token), "%08lx%08lx",
                 (unsigned long)esp_random(), (unsigned long)esp_random());
        httpd_start_ota();
        csm_emit("CSM WIFI ok ip=" IPSTR " token=%s\n",
                 IP2STR(&e->ip_info.ip), s_token);
    }
}

// ---- Veřejné API ----
void ota_wifi_connect(const char *ssid, const char *pass)
{
    if (!s_inited) {
        ESP_ERROR_CHECK(esp_netif_init());
        esp_err_t err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(err);
        }
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));  // creds jen v RAM
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   wifi_event, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   wifi_event, NULL));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        s_inited = true;
    }

    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));

    s_connected  = false;
    s_connecting = true;
    s_retries    = 0;
    csm_emit("CSM WIFI connecting\n");

    esp_wifi_stop();                              // čistý start (STA_START -> connect)
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void ota_wifi_off(void)
{
    s_connecting = false;
    s_connected  = false;
    httpd_stop_ota();
    if (s_inited) {
        esp_wifi_stop();
    }
    csm_emit("CSM WIFI off\n");
}
