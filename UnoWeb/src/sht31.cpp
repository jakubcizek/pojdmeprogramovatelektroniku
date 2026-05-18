#include "sht31.h"
#include <util/twi.h>

#define SHT31_ADDR    0x44
#define SHT31_CMD_MSB 0x24    // single-shot, clock stretching disabled
#define SHT31_CMD_LSB 0x00    // high repeatability

// ---- TWI nizkourovnove (polling TWINT, s timeoutem - server nesmi zamrznout) -
static bool twiWait() {
  for (uint16_t i = 0; i < 50000; i++)      // ~ms rad pri 100 kHz
    if (TWCR & _BV(TWINT)) return true;
  return false;
}

static bool twiStart() {
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  if (!twiWait()) return false;
  uint8_t s = TW_STATUS;
  return (s == TW_START || s == TW_REP_START);
}

static bool twiSla(uint8_t addr7, bool read) {
  TWDR = (uint8_t)((addr7 << 1) | (read ? 1 : 0));
  TWCR = _BV(TWINT) | _BV(TWEN);
  if (!twiWait()) return false;
  uint8_t s = TW_STATUS;
  return read ? (s == TW_MR_SLA_ACK) : (s == TW_MT_SLA_ACK);
}

static bool twiWrite(uint8_t b) {
  TWDR = b;
  TWCR = _BV(TWINT) | _BV(TWEN);
  if (!twiWait()) return false;
  return TW_STATUS == TW_MT_DATA_ACK;
}

static uint8_t twiRead(bool ack) {
  TWCR = _BV(TWINT) | _BV(TWEN) | (ack ? _BV(TWEA) : 0);
  twiWait();
  return TWDR;
}

static void twiStop() {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
}

// ---- SHT3x CRC-8 (poly 0x31, init 0xFF) -------------------------------------
static uint8_t crc8(uint8_t a, uint8_t b) {
  uint8_t c = 0xFF;
  c ^= a;
  for (uint8_t i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
  c ^= b;
  for (uint8_t i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
  return c;
}

void sht31_init() {
  // Interni pull-up na SDA(PC4)/SCL(PC5) jako pojistka k externim na desce.
  PORTC |= _BV(PC4) | _BV(PC5);
  TWSR = 0;          // prescaler = 1
  TWBR = 72;         // SCL = 16 MHz / (16 + 2*72) = 100 kHz
  TWCR = _BV(TWEN);

  delay(2);          // power-up senzoru (max ~1 ms)
  // Soft reset SHT3x (0x30A2) -> znamy stav nezavisle na predchozim debugu.
  if (twiStart() && twiSla(SHT31_ADDR, false)) {
    twiWrite(0x30);
    twiWrite(0xA2);
  }
  twiStop();
  delay(2);          // doba soft resetu (max ~1.5 ms)
}

// Jeden pokus o mereni; true = platna data.
static bool sht31_once(int16_t* t100, int16_t* rh100) {
  // 1) Vyslat 2bajtovy prikaz mereni 0x2400.
  if (!twiStart())                  { twiStop(); return false; }
  if (!twiSla(SHT31_ADDR, false))   { twiStop(); return false; }
  if (!twiWrite(SHT31_CMD_MSB))     { twiStop(); return false; }
  if (!twiWrite(SHT31_CMD_LSB))     { twiStop(); return false; }
  twiStop();

  delay(20);                        // doba prevodu (high rep. max ~15.5 ms)

  // 2) Precist 6 bajtu: T[2]+CRC, RH[2]+CRC.
  if (!twiStart())                  { twiStop(); return false; }
  if (!twiSla(SHT31_ADDR, true))    { twiStop(); return false; }
  uint8_t d[6];
  for (uint8_t i = 0; i < 6; i++) d[i] = twiRead(i < 5);   // posledni NACK
  twiStop();

  if (crc8(d[0], d[1]) != d[2]) return false;
  if (crc8(d[3], d[4]) != d[5]) return false;

  uint16_t st  = ((uint16_t)d[0] << 8) | d[1];
  uint16_t srh = ((uint16_t)d[3] << 8) | d[4];

  // SHT3x:  T[°C] = -45 + 175 * st/65535   -> x100
  //         RH[%] = 100 * srh/65535        -> x100 (bez offsetu), oriznuto 0..100
  *t100 = (int16_t)(-4500 + (int16_t)(((int32_t)17500 * st) / 65535));
  int16_t rh = (int16_t)(((int32_t)10000 * srh) / 65535);
  if (rh < 0)     rh = 0;
  if (rh > 10000) rh = 10000;
  *rh100 = rh;
  return true;
}

bool sht31_read(int16_t* t100, int16_t* rh100) {
  // Retry: smycka bezi jen pri selhani (uspesny pripad = nulova rezie navic).
  // Resi ztracenou prvni transakci po power-upu (proto to "slo jen v debugu").
  for (uint8_t a = 0; a < 3; a++) {
    if (sht31_once(t100, rh100)) return true;
    delay(3);
  }
  return false;
}

#ifdef APP_DEBUG
// Projde 7bit adresy 0x08..0x77, vypise ty, ktere ACKnou na SLA+W.
void i2c_scan() {
  // Klidove urovne sbernice: SDA=PC4, SCL=PC5. Obe musi byt 1 (pull-up).
  // SDA/SCL = 0 -> chybi pull-up / neni napajeni / zkrat / stuck.
  // SDA/SCL = 1 a presto 0 dev -> sbernice OK, senzor nereaguje (spatne
  //                               zapojeny senzor / vadny / spatna adresa).
  uint8_t pc = PINC;
  Serial.print(F("SDA="));
  Serial.print((pc >> PC4) & 1);
  Serial.print(F(" SCL="));
  Serial.println((pc >> PC5) & 1);

  Serial.println(F("I2C scan:"));
  uint8_t found = 0;
  for (uint8_t a = 0x08; a <= 0x77; a++) {
    bool ack = twiStart() && twiSla(a, false);
    twiStop();
    if (ack) {
      Serial.print(F("  0x"));
      Serial.println(a, HEX);
      found++;
    }
  }
  Serial.print(found);
  Serial.println(F(" dev"));
}
#endif
