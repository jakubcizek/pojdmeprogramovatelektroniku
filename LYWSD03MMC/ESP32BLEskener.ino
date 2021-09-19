/*
 * Tento projekt je urceny pro libovlnou prototypovaci desku s cipem ESP32
 * Projekt cte advertizacni data z teplomeru Xiaomi (LYWSD03MMC), na kterem je nahrany
 * komunitní firmware https://github.com/pvvx/ATC_MiThermometer
 * a do okoli vysila data ve formatu custom: https://github.com/pvvx/ATC_MiThermometer#custom-format-all-data-little-endian
 * (jedna se o vychozi stav tohoto firmwaru)
*/
#include <BLEDevice.h> // Vestavena knihovna BLE zarizeni
#include <BLEUtils.h> // Podpurna knihovna pro BLE
#include <BLEScan.h> // Knihovna pro BLE sken zarizeni v okoli
#include <BLEAdvertisedDevice.h> // Knihovna pro advertizacni zpravy BLE

// Promenne pro zachycene udaje z teplomeru
float teplota;
float vlhkost;
uint16_t napetiBaterie;
uint8_t nabitiBaterie;
uint8_t pocitadlo;
uint8_t mac[6];

// Ukazatel na tridu BLE skeneru
BLEScan* sken;

// Vlastni odvolzena trida pro zpracovani advertizacni zpravy
class NovaAdvertizacniData: public BLEAdvertisedDeviceCallbacks {

    // Privatni metoda tridy pro nalezeni dat sluzby v celkove zprave
    uint8_t* najdiData(uint8_t* data, size_t length, uint8_t* delka) {
      uint8_t* pravyOkraj = data + length;
      while (data < pravyOkraj) {
        uint8_t delkaBloku = *data + 1;
        if (delkaBloku < 5) {
          data += delkaBloku;
          continue;
        }
        uint8_t typBloku = *(data + 1);
        uint16_t typSluzby = *(uint16_t*)(data + 2);
        // Pokud se jedna o korektni typ dat a tridu s UUID 0x181a,
        // vrat data
        if (typBloku == 0x16) {
          if (typSluzby == 0x181a) {
            *delka = delkaBloku;
            return data;
          }
        }
        data += delkaBloku;
      }
      return nullptr;
    }

    // Zdedena metoda, ktera se zpracuje pri obdrzeni zpravy
    void onResult(BLEAdvertisedDevice zarizeni) {
      // Ziskej data
      uint8_t* data = zarizeni.getPayload();
      // Ziskej delku dat
      size_t delkaDat = zarizeni.getPayloadLength();
      uint8_t delkaDatSluzby = 0;
      // Vytahni ze surove zpravy data sluzby, ktera obsahuji nami hledane udaje
      uint8_t* dataSluzby = najdiData(data, delkaDat, &delkaDatSluzby);
      if (data == nullptr || delkaDatSluzby < 18) return;
      // Pokud maji data sluzby delku 19 bajtu, je to to, co hledame
      if (delkaDatSluzby > 18) {
        // 4. az 9. bajt dat obsahuje MAC adresu
        mac[5] = data[4];
        mac[4] = data[5];
        mac[3] = data[6];
        mac[2] = data[7];
        mac[1] = data[8];
        mac[0] = data[9];
        // Za MAC adresou nasleduje 16bitova teplota jako cele cislo se zamenkem nasobene 100
        // Napr. hodnota 2035 tedy odpovida 20,35 stupnum
        teplota = *(int16_t*)(data + 10) / 100.0;
        // Za teplotou nasleduje 16bitove cele cislo bez znamenka s rel. vlhkosti v procentech a opet znasobene 100
        vlhkost = *(uint16_t*)(data + 12) / 100.0;
        // Dalsich 16 bitu predstavuje napeti baterie v milivoltech
        napetiBaterie = *(uint16_t*)(data + 14);
        // Dalsich 8 bitů obsahuje nabiti baterie v procentech
        nabitiBaterie = data[16];
        // A konecne nasleduje 8bitove pocitadlo
        // BLE zpravy muze teplomer vysilat casteji nez provadi samotne mereni, pocitadlo proto pomuze zjistit,
        // zdali se jedna o novou hodnotu, nebo se jen opakuje
        pocitadlo = data[17];
        // Pro kontrolu vsechny hodnoty vypiseme po radcich a formatovane do seriove linky
        Serial.printf("Teplota: %.2f\r\nVlhkost: %.2f\r\nNapeti baterie: %u\r\nNabiti baterie: %d\r\nMAC: %02x:%02x:%02x:%02x:%02x:%02x\r\nPocitadlo: %d\r\n\r\n",
                      teplota, vlhkost, napetiBaterie, nabitiBaterie, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], pocitadlo);
      }
    }
};

// Hlavni funkce setup se zpracuje ihned na zacatku po pripojei zdroje napajeni
void setup() {
  // Nastartuj seriovou linku rychlosti 115 200 b/s 
  Serial.begin(115200);
  // Pockej sekundu
  delay(1000);
  // Vypis zpravu jako overeni, ze vse funguje
  Serial.println("Start programu...");
  // Nastartuj BLE prijimac
  BLEDevice::init("");
  // Inicializuj BLE skener
  sken = BLEDevice::getScan();
  // Po prichodu advertizacnich dat skener zavola nasi tridu, kterou jsme si vytvorili vyse
  sken->setAdvertisedDeviceCallbacks(new NovaAdvertizacniData(), true);
  // Dalsi parametry skeneru - nahlednete do dokumentace tridy
  sken->setInterval(625);
  sken->setWindow(625);
  sken->setActiveScan(true);
}

// Smycka loop se spusti po zpracovani funcke setup a opakuje se stale dokola
void loop() {
  // Spust skener a skenuj okoli po dobu 30 sekund
  // Pokud v tomto intervalu dorazi nejake advertizacni zpravy, zpracuje je asynchronne nase trida
  BLEScanResults foundDevices = sken->start(30, false);
  // Zastav skener
  sken->stop();
  // Smaz vysledky
  sken->clearResults();
}
