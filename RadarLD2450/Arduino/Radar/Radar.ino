/*

Prepracovany zdrojovy kod komponenty pro ESPHome od tsunglung
do podoby bezneho perogramu pro Arduino, ktery cte data z radaru
a vypisuje udaje o pohybu do seriove linky ve formatu>

id,x,y,rychlost

x a y je v centimetrech, rychlost v cm/s
id je index podle prom2nne sledovaneCile

Puvodni zdrojovy kod>:
https://github.com/tsunglung/esphome-ld2450/tree/master

*/


// Buffer pro data z radaru
uint8_t data[160];
// Kolik budeme sledovat cilu? 1 az 3
uint8_t sledovaneCile = 3;
// Prodleva mezi merenimi na 1000 ms
// Halvne rpo preheldnost, surova vychozi rychlost je zhruba 10 Hz
uint16_t minimalniProdleva = 1000;
uint32_t posledniAktualizace = millis();

// Funkce pro slocueni dvou bajtku na 16bit cislo
uint16_t sloucitDvaBajty(uint8_t prvni, uint8_t druhy) {
  return (uint16_t)(druhy << 8) + prvni;
}

// Vypsani informaci o pohybu do seriove linky
void vypsatStavCile(uint8_t cil, uint8_t *data) {
  int16_t x, y, rychlost;
  uint16_t rozliseni;
  x = sloucitDvaBajty(data[0], data[1] & 0x7F);
  if (data[1] >> 7 != 0x1) x = 0 - x / 10;
  else x = x / 10;
  y = sloucitDvaBajty(data[2], data[3] & 0x7F);
  if (data[3] >> 7 != 0x1) y = 0 - y / 10;
  else y = y / 10;
  rychlost = sloucitDvaBajty(data[4], data[5] & 0x7F);
  if (data[5] >> 7 != 0x1) rychlost = 0 - rychlost;
  rozliseni = sloucitDvaBajty(data[6], data[7]);
  Serial.printf("%d,%d,%d,%d\r\n", cil, x, y, rychlost);
}

// Zpracovani binarni zpravy uvozene hlavickou a zakoncene patickou
void zpracovatData(uint8_t *data, uint8_t delka) {
  if (delka < 29) return;  // Kontrola na delku prijate zpravy
  if (data[0] != 0xAA || data[1] != 0xFF || data[2] != 0x03 || data[3] != 0x00)
    return;  // Hlavicka musi obsahovat bajty 0xAA, 0xFF, 0x03 a 0x00
  if (data[delka - 2] != 0x55 || data[delka - 1] != 0xCC)
    return;  // Paticka musi koncit bajty 0x55 a 0xCC

  // Snizeni rychlosti cca na 1 Hz (minimalniProdleva = 1000 ms)
  uint32_t currentMillis = millis();
  if (millis() - posledniAktualizace < minimalniProdleva)
    return;
  posledniAktualizace = millis();

  // Radar podporuje sledovani az tri cilu,
  // a tak jeden po druhem vypiseme
  for (int cil = 0; cil < sledovaneCile; cil++) {
    uint8_t stav[8];
    memcpy(stav, &data[4 + cil * 8], 8);
    vypsatStavCile(cil, stav);
  }
}

// Zpracovani dat ze seriove linky znak po znaku
void precistData(uint8_t znak, uint8_t *data, int delka) {
  static uint8_t pozice = 0;
  if (znak >= 0) {
    if (pozice < delka - 1) {
      data[pozice++] = znak;
      data[pozice] = 0;
    } else {
      pozice = 0;
    }
    if (pozice >= 4) {
      // Pokud najdu v prectenych seriovych datech paticku 0x55 a 0xCC,
      // mam kompletni zpravu a mohu ji dale zpracovat
      if (data[pozice - 2] == 0x55 && data[pozice - 1] == 0xCC) {
        zpracovatData(data, pozice);
        pozice = 0;
      }
    }
  }
}

// V hlavni funkci setup nastartujeme dve seriove linky
// Serial pro vypis informaci do PC po USB
// Serial2 pro komuniakci s radarem
// Toto nastaveni plati pro ESP32-S3 na desce DevkitC
// Upravte dle vlastni potreby
void setup() {
  Serial.begin(115200);
  Serial2.begin(256000, SERIAL_8N1, 18, 17);  // 18 = RX,  17 = TX na ridici desce
}

// Ve smycce loop cekame na prichozi bajty z radaru 
// a posilame je ke zpracovani do funkce precistData
// Ta ma k dispozici 160B buffer data
void loop() {
  while (Serial2.available()) {
    precistData(Serial2.read(), data, 160);
  }
}
