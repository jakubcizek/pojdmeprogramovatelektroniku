// =============================================================================
//  UnoWeb - minimalni WWW server na ATMEGA328P (2 kB SRAM) + W5500 + SHT31
//
//  Architektura:
//    - kazdou minutu jedno mereni SHT31 -> 4 B v SRAM
//    - HTML kompletne ve flash (PROGMEM), streamovano primo do TX W5500
//    - statistika: pocet GET + prenesene bajty (rx/tx) -> 12 B v SRAM
//    - LAN IP pres DHCP, fallback na statickou IP
//
//  Optimalizace: raw SPI/TWI registry, CS pres PORTB, 1 socket, zadny String,
//  zadne knihovny Ethernet/Wire. Cela odpoved odejde jednim SEND.
// =============================================================================
#include <Arduino.h>
#include <avr/pgmspace.h>
#include "config.h"
#include "w5500.h"
#include "dhcp.h"
#include "sht31.h"
#include "page.h"

// ---- Stav v SRAM (aplikacni data; rozpocet ~38 B) ---------------------------
static int16_t  g_t100  = 0;     // teplota °C x100
static int16_t  g_rh100 = 0;     // vlhkost % x100
static uint32_t g_req   = 0;     // celkem HTTP GET dotazu od startu
static uint32_t g_page  = 0;     // z toho dotazu na stranku (GET / )
static uint32_t g_rx    = 0;     // prijate bajty (HTTP socket)
static uint32_t g_tx    = 0;     // odeslane bajty (HTTP socket)
static uint16_t g_hours = 0;     // hruby uptime v hodinach (0..65535)
static uint8_t  g_min   = 0;     // minuty v ramci aktualni hodiny

static uint8_t  g_ip[4], g_gw[4], g_sn[4];
static const uint8_t g_mac[6] = { MAC0, MAC1, MAC2, MAC3, MAC4, MAC5 };

static uint32_t g_lastMeas;
// Per-socket stav: +5 B na socket (4 B connStart + 1 B prevSr).
// NSOCK=1 -> stejne jako puvodni skalary.
static uint32_t g_connStart[NSOCK];
static uint8_t  g_prevSr[NSOCK];

// ---- TCP server: otevri socket sn a posad ho do LISTEN ----------------------
static void tcpListen(uint8_t sn) {
  wz_w8(WZ_Sn_MR, WZ_SREG_W(sn), WZ_MR_TCP);
  wz_w16(WZ_Sn_PORT, WZ_SREG_W(sn), HTTP_PORT);
  wz_cmd(sn, WZ_CR_OPEN);
  for (uint8_t i = 0; i < 50; i++) {
    if (wz_status(sn) == WZ_SR_INIT) break;
    delay(1);
  }
  wz_cmd(sn, WZ_CR_LISTEN);
}

// ---- Pomocnici pro skladani odpovedi primo do TX bufferu W5500 --------------
static uint16_t putP(uint16_t p, uint8_t txc, const char* pgm) {
  uint16_t len = strlen_P(pgm);
  wz_writeBuf(p, txc, (const uint8_t*)pgm, len, true);
  return p + len;
}

static uint16_t putU32(uint16_t p, uint8_t txc, uint32_t v) {
  char b[10];
  uint8_t i = 10;
  if (v == 0) b[--i] = '0';
  while (v) { b[--i] = (char)('0' + v % 10); v /= 10; }
  uint8_t len = 10 - i;
  wz_writeBuf(p, txc, (const uint8_t*)&b[i], len, false);
  return p + len;
}

// Hodnota x100 -> "[-]int.frac" (2 desetinna mista).
static uint16_t putFixed(uint16_t p, uint8_t txc, int16_t x100) {
  char b[8];
  uint8_t i = 8;
  bool neg = x100 < 0;
  uint16_t a = neg ? (uint16_t)(-x100) : (uint16_t)x100;
  uint16_t ip = a / 100, fr = a % 100;
  b[--i] = (char)('0' + fr % 10);
  b[--i] = (char)('0' + fr / 10);
  b[--i] = '.';
  if (ip == 0) b[--i] = '0';
  else while (ip) { b[--i] = (char)('0' + ip % 10); ip /= 10; }
  if (neg) b[--i] = '-';
  uint8_t len = 8 - i;
  wz_writeBuf(p, txc, (const uint8_t*)&b[i], len, false);
  return p + len;
}

static void sendPage(uint8_t sn) {
  uint8_t  txc = WZ_STX_W(sn);
  uint16_t start = wz_txWr(sn);
  uint16_t p = start;
  p = putP   (p, txc, PG_HEAD);
  p = putFixed(p, txc, g_t100);
  p = putP   (p, txc, PG_T2H);
  p = putFixed(p, txc, g_rh100);
  p = putP   (p, txc, PG_H2S);
  p = putU32 (p, txc, g_page);
  p = putP   (p, txc, PG_HITS);
  p = putU32 (p, txc, g_req);
  p = putP   (p, txc, PG_TOT);
  p = putU32 (p, txc, g_rx);
  p = putP   (p, txc, PG_RX);
  p = putU32 (p, txc, g_tx);
  p = putP   (p, txc, PG_UPT);
  p = putU32 (p, txc, g_hours);
  p = putP   (p, txc, PG_TAIL);
  g_tx += (uint16_t)(p - start);
  wz_send(sn, p);
}

// Minimalni 404 pro cesty != "/" (favicon.ico apod.).
static void send404(uint8_t sn) {
  uint16_t start = wz_txWr(sn);
  uint16_t p = putP(start, WZ_STX_W(sn), PG_404);
  g_tx += (uint16_t)(p - start);
  wz_send(sn, p);
}

// ---- Obsluha jednoho socketu (volano pro kazdy z NSOCK v loop) --------------
static void serviceSocket(uint8_t sn) {
  uint8_t sr = wz_status(sn);

  if (sr == WZ_SR_CLOSED) {
    tcpListen(sn);
  } else if (sr == WZ_SR_ESTABLISHED || sr == WZ_SR_CLOSE_WAIT) {
    if (g_prevSr[sn] != WZ_SR_ESTABLISHED && g_prevSr[sn] != WZ_SR_CLOSE_WAIT)
      g_connStart[sn] = millis();

    uint16_t avail = wz_rxAvail(sn);
    if (avail) {
      // "Peek" prvnich 6 bajtu ("GET / ") kvuli rozliseni stranka/HTTP,
      // pak cely request zahodime.
      uint16_t rd = wz_r16(WZ_Sn_RX_RD, WZ_SREG_R(sn));
      uint8_t  g[6] = { 0, 0, 0, 0, 0, 0 };
      wz_readBuf(rd, WZ_SRX_R(sn), g, avail < 6 ? (uint8_t)avail : 6);
      g_rx += avail;
      wz_rxConsume(sn, avail);

      bool isRoot = false;
      if (g[0] == 'G' && g[1] == 'E' && g[2] == 'T') {
        g_req++;                                    // celkem HTTP GET
        if (g[3] == ' ' && g[4] == '/' && g[5] == ' ') {
          g_page++;                                 // GET / = dotaz na stranku
          isRoot = true;
        }
      }
      if (isRoot) sendPage(sn);         // stranka jen pro GET /
      else        send404(sn);          // /favicon.ico apod. -> minimalni 404

      wz_cmd(sn, WZ_CR_DISCON);
      for (uint8_t i = 0; i < 30; i++) {       // kratke cekani na uzavreni
        if (wz_status(sn) == WZ_SR_CLOSED) break;
        delay(2);
      }
      wz_cmd(sn, WZ_CR_CLOSE);
      tcpListen(sn);
    } else if (sr == WZ_SR_CLOSE_WAIT ||
               (uint16_t)(millis() - g_connStart[sn]) > 3000) {
      // Peer zavrel bez dat, nebo scanner drzi spojeni -> zahodit.
      wz_cmd(sn, WZ_CR_DISCON);
      wz_cmd(sn, WZ_CR_CLOSE);
      tcpListen(sn);
    }
  }
  g_prevSr[sn] = sr;
}

void setup() {
  DBG_BEGIN();
  wz_spiInit();

  for (uint8_t i = 0; i < 5 && !wz_reset(); i++) delay(100);
  DBG(F("W5500 ver=")); DBG_LN(wz_version());

  // Pockej na ethernet link (max ~5 s) - bez linku DHCP nema smysl.
  for (uint8_t i = 0; i < 50 && !wz_linkUp(); i++) delay(100);
  DBG(F("link=")); DBG_LN(wz_linkUp() ? F("up") : F("down"));

  wz_setMac(g_mac);   // MAC do SHAR PRED DHCP, jinak src MAC = 00:00:00:00:00:00

  // LAN IP pres DHCP, jinak fallback staticka.
  if (dhcp_acquire(g_mac, g_ip, g_gw, g_sn)) {
    DBG_LN(F("DHCP ok"));
  } else {
    g_ip[0]=FALLBACK_IP_0; g_ip[1]=FALLBACK_IP_1; g_ip[2]=FALLBACK_IP_2; g_ip[3]=FALLBACK_IP_3;
    g_gw[0]=FALLBACK_GW_0; g_gw[1]=FALLBACK_GW_1; g_gw[2]=FALLBACK_GW_2; g_gw[3]=FALLBACK_GW_3;
    g_sn[0]=FALLBACK_SN_0; g_sn[1]=FALLBACK_SN_1; g_sn[2]=FALLBACK_SN_2; g_sn[3]=FALLBACK_SN_3;
    DBG_LN(F("DHCP fail -> static"));
  }
  wz_setNet(g_ip, g_gw, g_sn, g_mac);
#ifdef APP_DEBUG
  Serial.print(g_ip[0]); Serial.print('.'); Serial.print(g_ip[1]);
  Serial.print('.'); Serial.print(g_ip[2]); Serial.print('.');
  Serial.println(g_ip[3]);
#endif

  sht31_init();
#ifdef APP_DEBUG
  i2c_scan();                                 // diagnostika: co je na sbernici
#endif
  bool sok = sht31_read(&g_t100, &g_rh100);   // prvni mereni hned
  (void)sok;                                  // v release je DBG no-op
  DBG(F("SHT31 ")); DBG(sok ? F("ok t=") : F("fail t="));
  DBG(g_t100); DBG(F(" rh=")); DBG_LN(g_rh100);
  g_lastMeas = millis();

  // Vsech NSOCK socketu do LISTEN na portu 80 (HW backlog -> mene odmitnuti).
  for (uint8_t sn = 0; sn < NSOCK; sn++) {
    g_prevSr[sn] = 0xFF;
    tcpListen(sn);
  }
  DBG(F("LISTEN sockets=")); DBG_LN(NSOCK);
}

void loop() {
  if ((uint32_t)(millis() - g_lastMeas) >= MEASURE_INTERVAL_MS) {
    g_lastMeas += MEASURE_INTERVAL_MS;
    sht31_read(&g_t100, &g_rh100);        // pri chybe ponecha posledni hodnoty
    if (++g_min >= TICKS_PER_HOUR) {      // hruby uptime, bez millis matematiky
      g_min = 0;
      g_hours++;
    }
  }
  for (uint8_t sn = 0; sn < NSOCK; sn++)
    serviceSocket(sn);
}
