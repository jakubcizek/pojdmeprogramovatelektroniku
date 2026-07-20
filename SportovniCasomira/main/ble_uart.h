/*
 * BLE "UART" - Nordic UART Service (NUS) pro bezdrátové ovládání časomíry
 * přes Web Bluetooth. Posílá stejné textové řádky protokolu CSM jako fyzická
 * UART a přijímá stejné příkazy (např. 'r' = nový start).
 *
 * NUS UUID (kompatibilní s Web Bluetooth):
 *   Service : 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX (W)  : 6E400002-...   web -> deska (příkazy)
 *   TX (N)  : 6E400003-...   deska -> web (notifikace, řádky CSM)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

// Callback volaný při příchodu bajtů z webu (z RX charakteristiky).
typedef void (*ble_uart_rx_cb_t)(const uint8_t *data, size_t len);

// Callback o změně připojení klienta (true = připojen, false = odpojen).
typedef void (*ble_uart_conn_cb_t)(bool connected);

// Spustí BLE stack, GATT server (NUS) a začne inzerovat pod daným jménem.
void ble_uart_init(const char *device_name);

// Registruje callback pro příchozí data (příkazy z webu).
void ble_uart_set_rx_cb(ble_uart_rx_cb_t cb);

// Registruje callback o (od)pojení klienta (pro reset stavu ověření apod.).
void ble_uart_set_conn_cb(ble_uart_conn_cb_t cb);

// Odešle data připojenému klientovi jako notifikaci (no-op, když nikdo není
// připojen nebo si notifikace nezapnul). Bezpečné volat i bez připojení.
void ble_uart_send(const char *data, size_t len);
