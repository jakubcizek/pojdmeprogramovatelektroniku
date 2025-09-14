#include "wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "Component/Wifi";
static EventGroupHandle_t s_wifi_event_group;
static int wifi_connect_count = 0;
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

//-----------------------------------------------------------------------------
// Funkce pro připojení k Wi-Fi
// Konfigurace Wi-Fi včetně SSID a hesla je v include/wifi.h
//-----------------------------------------------------------------------------
void wifi_connect(void){
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH,
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Připojeno k síti %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Nepodařilo se připojit k síti %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Wtf? Nevím, dál!");
    }
}

//-----------------------------------------------------------------------------
// Funkce pro zachycení systémových událostí během připojování
// Zachycení systémových událostí během připojování
// Podle nich se rozhodneme, jestli jsme připojení, nebo ne
// řešíme zde i opakovaný pokus o připojení, pokude se to nedaří
//-----------------------------------------------------------------------------
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if ((WIFI_CONNECT_RETRY_MAX > 0) && (wifi_connect_count < WIFI_CONNECT_RETRY_MAX)) {
            esp_wifi_connect();
            wifi_connect_count++;
            ESP_LOGI(TAG, "%d. pokus o připojení %s", wifi_connect_count, WIFI_SSID);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"Připojení k Wi-Fi selhalo");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP adresa čipu:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Server bude dostupný na adrese http://" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connect_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}