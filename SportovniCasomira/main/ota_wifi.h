/*
 * OTA aktualizace firmwaru přes Wi-Fi.
 *
 * Tok: web pošle příkaz "WIFI <ssid>\t<heslo>" -> deska se připojí jako STA,
 * spustí HTTP server s endpointem POST /ota a ohlásí "CSM WIFI ok ip=<ip>
 * token=<t>". Web pak nahraje .bin přímo přes HTTP (hlavička X-Auth=<token>).
 * Po úspěšném zápisu deska ohlásí "CSM OTA done" a restartuje se.
 *
 * SSID/heslo se drží jen v RAM (WIFI_STORAGE_RAM) - nikdy se neukládají.
 */
#pragma once

// Připojí se k Wi-Fi (STA). Výsledek hlásí asynchronně řádky CSM WIFI ...
void ota_wifi_connect(const char *ssid, const char *pass);

// Vypne Wi-Fi a HTTP server. Ohlásí "CSM WIFI off".
void ota_wifi_off(void);
