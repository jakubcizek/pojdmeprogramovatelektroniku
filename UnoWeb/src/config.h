#pragma once
#include <Arduino.h>

// =============================================================================
//  UnoWeb - centralni konfigurace
// =============================================================================
//
//  Deska: Keyestudio W5500 Ethernet Development Board
//         (Arduino UNO / ATMEGA328P + WIZnet W5500 pres SPI)
//  Senzor: Sensirion SHT31 (I2C / TWI)
//
// -----------------------------------------------------------------------------
//  Pinout (ATMEGA328P)
// -----------------------------------------------------------------------------
//  W5500 SCS (chip select) -> D10 / PB2   (CS ovladame primo pres PORTB)
//  W5500 MOSI              -> D11 / PB3
//  W5500 MISO              -> D12 / PB4
//  W5500 SCK               -> D13 / PB5
//  SHT31 SDA               -> A4         (TWI)
//  SHT31 SCL               -> A5         (TWI)
//
//  W5500 RST: pin nebyl potvrzen -> pouzivame SOFTWAROVY reset pres registr MR.
//  Pokud ma tva deska RST vyvedeny na GPIO, odkomentuj a nastav cislo pinu:
// #define W5500_RST_PIN 9
// =============================================================================

// ---- W5500 CS na PB2 (D10) - primy port pristup misto digitalWrite ----------
#define W5500_CS_DDR    DDRB
#define W5500_CS_PORT   PORTB
#define W5500_CS_BIT    PB2

// ---- MAC adresa -------------------------------------------------------------
// W5500 nema vlastni MAC, musime ji priradit. Locally administered adresa
// (druhy nejnizsi bit prvniho bajtu = 1). Zmen, pokud mas v siti vic kusu.
#define MAC0 0xDE
#define MAC1 0xAD
#define MAC2 0xBE
#define MAC3 0xEF
#define MAC4 0xFE
#define MAC5 0xED

// ---- Fallback staticka IP ---------------------------------------------------
// LAN IP ziskavame pres DHCP. Pokud DHCP selze (po nekolika pokusech),
// pouzije se tato zaloha, aby zarizeni zustalo dosazitelne.
#define FALLBACK_IP_0   192
#define FALLBACK_IP_1   168
#define FALLBACK_IP_2   1
#define FALLBACK_IP_3   50
#define FALLBACK_GW_0   192
#define FALLBACK_GW_1   168
#define FALLBACK_GW_2   1
#define FALLBACK_GW_3   1
#define FALLBACK_SN_0   255
#define FALLBACK_SN_1   255
#define FALLBACK_SN_2   255
#define FALLBACK_SN_3   0

// ---- HTTP -------------------------------------------------------------------
#define HTTP_PORT       80

// Pocet soubeznych TCP socketu W5500 v LISTEN (1..8). Skaluje robustnost
// proti zatezi vs. RAM. Kazdy socket nad ramec prvniho stoji +5 B SRAM
// (uint32_t connStart + uint8_t prevSr). NSOCK=1 -> RAM jako puvodne.
// Doporuceno 4 pro verejne vystaveni (kompromis robustnost/RAM).
#define NSOCK           8

// ---- Mereni -----------------------------------------------------------------
#define MEASURE_INTERVAL_MS 60000UL   // 1 minuta
// Kolik tiku mereni = 1 hodina (pro hruby uptime, bez dalsi millis matematiky).
#define TICKS_PER_HOUR (3600000UL / MEASURE_INTERVAL_MS)

// ---- Volitelny serlovy debug ------------------------------------------------
// Odkomentuj pro diagnostiku pres Serial @ 115200 (stoji flash + trochu SRAM).
// #define APP_DEBUG

#ifdef APP_DEBUG
  #define DBG_BEGIN()   Serial.begin(115200)
  #define DBG(x)        Serial.print(x)
  #define DBG_LN(x)     Serial.println(x)
#else
  #define DBG_BEGIN()
  #define DBG(x)
  #define DBG_LN(x)
#endif
