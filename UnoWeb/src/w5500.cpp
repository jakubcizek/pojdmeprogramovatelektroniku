#include "w5500.h"
#include "config.h"

// ---- SPI primitiva (primy SPDR, bez Arduino SPI knihovny) -------------------
static inline uint8_t spi(uint8_t b) {
  SPDR = b;
  // Tesna smycka -> -Os generuje prakticky optimalni out/in SPDR.
  while (!(SPSR & _BV(SPIF))) { }
  return SPDR;
}

// CS primo pres port (zadny digitalWrite) - usetri ~us na kazde transakci.
#define CS_LOW()   (W5500_CS_PORT &= (uint8_t)~_BV(W5500_CS_BIT))
#define CS_HIGH()  (W5500_CS_PORT |=  (uint8_t)_BV(W5500_CS_BIT))

void wz_spiInit() {
  // CS jako vystup, klidova uroven HIGH.
  W5500_CS_DDR  |= _BV(W5500_CS_BIT);
  CS_HIGH();
  // PB3 MOSI, PB5 SCK, PB2 SS -> vystup; PB4 MISO -> vstup.
  DDRB |=  _BV(PB3) | _BV(PB5) | _BV(PB2);
  DDRB &= ~_BV(PB4);
  // SPI master, mode 0, MSB first. SPI2X -> fck/2 = 8 MHz @ 16 MHz (max AVR).
  SPCR = _BV(SPE) | _BV(MSTR);
  SPSR |= _BV(SPI2X);
}

// ---- Blokove cteni / zapis (jedna CS-low transakce) ------------------------
void wz_writeBuf(uint16_t addr, uint8_t ctrl, const uint8_t* p,
                 uint16_t len, bool fromPgm) {
  CS_LOW();
  spi((uint8_t)(addr >> 8));
  spi((uint8_t)addr);
  spi(ctrl);
  while (len--) spi(fromPgm ? pgm_read_byte(p++) : *p++);
  CS_HIGH();
}

// Zapise 'len' stejnych bajtu bez velkeho RAM bufferu (vyplne v DHCP packetu).
void wz_writeFill(uint16_t addr, uint8_t ctrl, uint8_t val, uint16_t len) {
  CS_LOW();
  spi((uint8_t)(addr >> 8));
  spi((uint8_t)addr);
  spi(ctrl);
  while (len--) spi(val);
  CS_HIGH();
}

void wz_readBuf(uint16_t addr, uint8_t ctrl, uint8_t* p, uint16_t len) {
  CS_LOW();
  spi((uint8_t)(addr >> 8));
  spi((uint8_t)addr);
  spi(ctrl);
  while (len--) *p++ = spi(0x00);
  CS_HIGH();
}

void wz_w8(uint16_t addr, uint8_t ctrl, uint8_t v) {
  wz_writeBuf(addr, ctrl, &v, 1, false);
}

uint8_t wz_r8(uint16_t addr, uint8_t ctrl) {
  uint8_t v;
  wz_readBuf(addr, ctrl, &v, 1);
  return v;
}

void wz_w16(uint16_t addr, uint8_t ctrl, uint16_t v) {
  uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
  wz_writeBuf(addr, ctrl, b, 2, false);
}

// Stabilni cteni 16b registru: pointery (RSR/FSR/WR) se muzou menit za behu,
// proto cteme dokud dve po sobe jdouci cteni nesouhlasi.
uint16_t wz_r16(uint16_t addr, uint8_t ctrl) {
  uint8_t b[2];
  uint16_t v, prev;
  wz_readBuf(addr, ctrl, b, 2);
  prev = ((uint16_t)b[0] << 8) | b[1];
  for (;;) {
    wz_readBuf(addr, ctrl, b, 2);
    v = ((uint16_t)b[0] << 8) | b[1];
    if (v == prev) return v;
    prev = v;
  }
}

// ---- Reset / verze / sit ----------------------------------------------------
bool wz_reset() {
#ifdef W5500_RST_PIN
  pinMode(W5500_RST_PIN, OUTPUT);
  digitalWrite(W5500_RST_PIN, LOW);
  delay(2);
  digitalWrite(W5500_RST_PIN, HIGH);
  delay(2);
#endif
  // Softwarovy reset: MR bit7 (RST). Cip ho po dokonceni sam vynuluje.
  wz_w8(WZ_MR, WZ_COMMON_W, 0x80);
  for (uint8_t i = 0; i < 50; i++) {        // ~50 ms timeout
    if (!(wz_r8(WZ_MR, WZ_COMMON_R) & 0x80)) break;
    delay(1);
  }
  delay(1);
  return wz_version() == 0x04;              // W5500 VERSIONR == 0x04
}

uint8_t wz_version() {
  return wz_r8(WZ_VERSIONR, WZ_COMMON_R);
}

bool wz_linkUp() {
  return wz_r8(WZ_PHYCFGR, WZ_COMMON_R) & 0x01;
}

void wz_setMac(const uint8_t mac[6]) {
  wz_writeBuf(WZ_SHAR, WZ_COMMON_W, mac, 6, false);
}

void wz_setNet(const uint8_t ip[4], const uint8_t gw[4],
               const uint8_t sn[4], const uint8_t mac[6]) {
  wz_writeBuf(WZ_SHAR, WZ_COMMON_W, mac, 6, false);
  wz_writeBuf(WZ_GAR,  WZ_COMMON_W, gw, 4, false);
  wz_writeBuf(WZ_SUBR, WZ_COMMON_W, sn, 4, false);
  wz_writeBuf(WZ_SIPR, WZ_COMMON_W, ip, 4, false);
  // Socket0 buffery 2 kB TX / 2 kB RX (default, nastaveno explicitne).
  wz_w8(WZ_Sn_TXBUF_SZ, WZ_S0_REG_W, 2);
  wz_w8(WZ_Sn_RXBUF_SZ, WZ_S0_REG_W, 2);
}

// ---- Socket pomocnici (sn = index socketu) ----------------------------------
void wz_cmd(uint8_t sn, uint8_t cmd) {
  wz_w8(WZ_Sn_CR, WZ_SREG_W(sn), cmd);
  // W5500 vynuluje Sn_CR po zpracovani prikazu.
  while (wz_r8(WZ_Sn_CR, WZ_SREG_R(sn))) { }
}

uint8_t wz_status(uint8_t sn) {
  return wz_r8(WZ_Sn_SR, WZ_SREG_R(sn));
}

uint16_t wz_rxAvail(uint8_t sn) {
  return wz_r16(WZ_Sn_RX_RSR, WZ_SREG_R(sn));
}

void wz_rxConsume(uint8_t sn, uint16_t len) {
  uint16_t rd = wz_r16(WZ_Sn_RX_RD, WZ_SREG_R(sn));
  rd += len;
  wz_w16(WZ_Sn_RX_RD, WZ_SREG_W(sn), rd);
  wz_cmd(sn, WZ_CR_RECV);
}

uint16_t wz_txWr(uint8_t sn) {
  return wz_r16(WZ_Sn_TX_WR, WZ_SREG_R(sn));
}

bool wz_send(uint8_t sn, uint16_t newTxWr) {
  wz_w16(WZ_Sn_TX_WR, WZ_SREG_W(sn), newTxWr);
  wz_w8(WZ_Sn_IR, WZ_SREG_W(sn), WZ_IR_SEND_OK | WZ_IR_TIMEOUT);  // smaz pred
  wz_cmd(sn, WZ_CR_SEND);
  for (uint16_t i = 0; i < 1000; i++) {     // ~1 s timeout
    uint8_t ir = wz_r8(WZ_Sn_IR, WZ_SREG_R(sn));
    if (ir & WZ_IR_SEND_OK) {
      wz_w8(WZ_Sn_IR, WZ_SREG_W(sn), WZ_IR_SEND_OK);
      return true;
    }
    if (ir & WZ_IR_TIMEOUT) {
      wz_w8(WZ_Sn_IR, WZ_SREG_W(sn), WZ_IR_TIMEOUT);
      return false;
    }
    delay(1);
  }
  return false;
}
