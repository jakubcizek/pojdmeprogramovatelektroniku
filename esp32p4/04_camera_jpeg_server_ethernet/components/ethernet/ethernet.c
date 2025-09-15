#include "esp_log.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_eth_driver.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"

#include "ethernet.h"

static const char *TAG = "Component/Ethernet";

// Deklarace privátních funkcí
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_eth_handle_t eth_init_internal(esp_eth_mac_t **mac_out, esp_eth_phy_t **phy_out);

//-----------------------------------------------------------------------------
// Funkce pro připojení k místní síti LAN skrze ethernet
// IP nám přidělí DHCP server
//-----------------------------------------------------------------------------
void ethernet_connect(void){
    // Inicializace síťového rozhraní
    // Používáme interní 
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_eth_handle_t eth_handle = eth_init_internal(NULL, NULL);
    ESP_ERROR_CHECK(eth_handle ? ESP_OK : ESP_FAIL);
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    ESP_ERROR_CHECK(eth_netif ? ESP_OK : ESP_FAIL);
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(glue ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, glue));

    // Handlery pro události ethernetu a IP stacku
    // První handler nás bude informovat, co se děje na úrovni PHY/MAC, no a druhý, jestli už máme IP adresu od místního DHCP
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // start Ethernetu
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}

//-----------------------------------------------------------------------------
// Handler pro událost IP stacku "IP_EVENT_ETH_GOT_IP"
// Jakmile ho systém zavolá, víme, že už máme IP adresu
//-----------------------------------------------------------------------------
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(TAG, "Připojeno k síti");
    ESP_LOGI(TAG, "IP adresa čipu:" IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Server bude dostupný na adrese http://" IPSTR, IP2STR(&event->ip_info.ip));
}

//-----------------------------------------------------------------------------
// Handler pro všechny události PHY/MAC stacku
// Systém ho zavolá při událostech linka on/off, ethernet start/stop aj.
//-----------------------------------------------------------------------------
static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernetová linka připojená");
        ESP_LOGI(TAG, "MAC adresa %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernetová linka odpojená");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Start ethernetu");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Zastavení ethernetu");
        break;
    default:
        ESP_LOGI(TAG, "Ethernetová událost č. %d", event_id);
        break;
    }
}

//-----------------------------------------------------------------------------
// Funkce pro inicializaci a start ethernetu pomocí internýho rozhraní MAC
// a externího rozhraní PHY ve formě čipu IP101 na desce 
//-----------------------------------------------------------------------------
static esp_eth_handle_t eth_init_internal(esp_eth_mac_t **mac_out, esp_eth_phy_t **phy_out){
    esp_eth_handle_t ret = NULL;

    // Výchozí konfigurace pro MAC (linkovou) a PHY (fyzickou) vrstvu ethernetu
    // MAC umí nativně ESP32-P4, PHY musí dodat další čip (bude to IP101, viz níže)
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Konfigurace GPIO, které propojují MAC a PHY
    // Deska od Wavesheru má všechny periferie zapojené stejně jako oficiáln í devkit
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = 51;
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdc_num = 31;
    esp32_emac_config.smi_gpio.mdio_num = 52;

    // Vytvoříme MAC rozhraní
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    
    // KLíčové! Jako fyzické (PHY) rozhraní používáme čip IP101 na desce od Wavesharu (stejný je i na velkém devkitu od Espressifu)
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    // Tak, už máme MAC a PHY konfiguraci, tak konečně nainstalujeme celý driver síťovky od Espressifu
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&config, &eth_handle) == ESP_OK, NULL, err, TAG, "Instalace ovladače ethernetu selhala");

    if (mac_out != NULL) {
        *mac_out = mac;
    }
    if (phy_out != NULL) {
        *phy_out = phy;
    }
    return eth_handle;
err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return ret;
}