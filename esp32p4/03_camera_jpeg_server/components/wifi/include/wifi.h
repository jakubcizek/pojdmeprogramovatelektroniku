#ifndef WIFI_H
#define WIFI_H

#include "sdkconfig.h"

// Nastavte SSID a heslo sítě
#define WIFI_SSID               CONFIG_WIFI_SSID
#define WIFI_PASSWORD           CONFIG_WIFI_PASSWORD

// Předpokládáme, že se jedná o síť se zabezpečním přinejmenším WPA2
#define WIFI_AUTH               WIFI_AUTH_WPA2_PSK

// Počet pokusů o připojení (0 = nekonečno)
#define WIFI_CONNECT_RETRY_MAX  0

// Pomocné hodnoty/bity pro určení stavu připojování
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

// Deklarace veřejných funkcí komponenty
void wifi_connect(void);

#endif

