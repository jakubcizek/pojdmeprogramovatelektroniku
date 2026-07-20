/*
 * BLE "UART" (Nordic UART Service) na NimBLE hostu.
 * Viz ble_uart.h pro popis rozhraní a UUID.
 */

#include <string.h>

#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ble_uart.h"

static const char *TAG = "ble_uart";

static uint16_t          s_conn_handle   = BLE_HS_CONN_HANDLE_NONE;
static uint16_t          s_tx_val_handle = 0;      // handle TX charakteristiky (notify)
static bool              s_subscribed    = false;  // klient si zapnul notifikace
static ble_uart_rx_cb_t   s_rx_cb        = NULL;
static ble_uart_conn_cb_t s_conn_cb      = NULL;
static uint8_t           s_addr_type;
static char              s_dev_name[32]  = "Casomira";

static void start_advertising(void);

// ---- NUS UUID (128-bit, v BLE_UUID128_INIT se uvádí od nejnižšího bajtu) ----
// 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t UUID_SVC = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
// 6E400002-... (RX, web -> deska, zápis)
static const ble_uuid128_t UUID_RX = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
// 6E400003-... (TX, deska -> web, notifikace)
static const ble_uuid128_t UUID_TX = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

// ---- Přístup ke GATT charakteristikám ----
static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Zápis do RX = příkaz z webu. Projdi řetěz mbuf a předej callbacku.
        for (struct os_mbuf *om = ctxt->om; om != NULL; om = SLIST_NEXT(om, om_next)) {
            if (s_rx_cb && om->om_len > 0) {
                s_rx_cb(om->om_data, om->om_len);
            }
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def GATT_SVCS[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid      = &UUID_RX.u,
                .access_cb = gatt_access,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid       = &UUID_TX.u,
                .access_cb  = gatt_access,
                .val_handle = &s_tx_val_handle,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }   // konec charakteristik
        },
    },
    { 0 }           // konec služeb
};

// ---- GAP události ----
static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "klient pripojen");
            if (s_conn_cb) s_conn_cb(true);
        } else {
            start_advertising();   // připojení selhalo, inzeruj dál
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "klient odpojen (reason=%d)", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_subscribed  = false;
        if (s_conn_cb) s_conn_cb(false);
        start_advertising();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_tx_val_handle) {
            s_subscribed = event->subscribe.cur_notify;
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;

    default:
        break;
    }
    return 0;
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)s_dev_name;
    fields.name_len = strlen(s_dev_name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;   // připojitelné
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;   // obecně objevitelné

    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start rc=%d", rc);
    }
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    if (ble_hs_id_infer_auto(0, &s_addr_type) != 0) {
        ESP_LOGE(TAG, "nelze urcit adresu");
        return;
    }
    start_advertising();
    ESP_LOGI(TAG, "BLE inzeruje jako \"%s\"", s_dev_name);
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

static void host_task(void *param)
{
    nimble_port_run();               // běží, dokud se stack nezastaví
    nimble_port_freertos_deinit();
}

// ================== Veřejné API ==================
void ble_uart_set_rx_cb(ble_uart_rx_cb_t cb)
{
    s_rx_cb = cb;
}

void ble_uart_set_conn_cb(ble_uart_conn_cb_t cb)
{
    s_conn_cb = cb;
}

void ble_uart_send(const char *data, size_t len)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_subscribed || s_tx_val_handle == 0) {
        return;   // nikdo připojený / notifikace nezapnuté
    }
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return;
    }
    ble_gatts_notify_custom(s_conn_handle, s_tx_val_handle, om);
}

void ble_uart_init(const char *device_name)
{
    if (device_name != NULL && device_name[0] != '\0') {
        strncpy(s_dev_name, device_name, sizeof(s_dev_name) - 1);
        s_dev_name[sizeof(s_dev_name) - 1] = '\0';
    }

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ESP_ERROR_CHECK(ble_gatts_count_cfg(GATT_SVCS));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(GATT_SVCS));

    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(s_dev_name));

    // Preferované ATT MTU ~200 B, ať se řádky CSM vejdou do jedné notifikace.
    ble_att_set_preferred_mtu(200);

    nimble_port_freertos_init(host_task);
}
