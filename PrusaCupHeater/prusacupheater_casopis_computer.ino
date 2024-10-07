// Zakladni demonstrace vyhrivani heatbed tile pro Original Prusa XL
// Pouzitiy hardware:
// - Heatbed tile (https://www.prusa3d.com/cs/produkt/heatbed-tile/)
// - Klon Arduino Nano
// - 0.96" OLED, 128x64 px, monochromaticky, s radicem SSD1306 (https://www.laskakit.cz/laskakit-oled-displej-128x64-1-3--i2c/)
// - Teplomer DS18B20
// - Mosfetovy spinac LR7843 (https://www.laskakit.cz/pwm-mosfet-modul-lr7843--30vdc-161a/)
// - Rotacni enkoder s tlacitkem (https://www.laskakit.cz/keyes-ky-040-rotacni-encoder-s-tlacitkem/)
// - 20V USB-C PD prepinac (https://www.laskakit.cz/laskakit-usb-c-pd-ch224k-prepinac-napajeciho-napeti/)
// - Volitelne: OpenLog pro snadne logovani na microSD (https://www.laskakit.cz/openlog-microsd-serial-data-logger/)

#include <Wire.h>                          // Knihovna pro sbernici I2C (komunikace displeje)
#include <Adafruit_GFX.h>                  // Prace s grafikou, https://github.com/adafruit/Adafruit-GFX-Library// Prace s grafikou, https://github.com/adafruit/Adafruit-GFX-Library
#include <Fonts/FreeMonoBold18pt7b.h>      // Velky font, soucast Adafruit GFX
#include <Fonts/FreeMonoBold9pt7b.h>       // Maly font, soucast Adafruit GFX
#include <Adafruit_SSD1306.h>              // Prace s 0.96" OLED s radicem SSD1306, https://github.com/adafruit/Adafruit_SSD1306
#include <RotaryEncoder.h>                 // Prace s rotacnim enkoderem
#include <OneWire.h>                       // Knihovna pro sbernici OneWire, https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>             // Knihovna pro praci s teplomerem DS18B20, https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <SoftwareSerial.h>                // Softwarova seriova linka pro ´komuniakci s deskou OpenLog

#define GPIO_ONEWIRE A0                    // Pin sbernice OneWire
#define GPIO_TOPNE_TELESO A1               // Pin mosfetoveho spinace napajeni heatbedu
#define GPIO_ENKODER_1 2                   // Pin A rotacniho enkoderu
#define GPIO_ENKODER_2 3                   // Pin B rotacniho enkoderu
#define GPIO_TLACITKO 4                    // Pin tlacitka rotacniho enkoderu
#define GPIO_OPENLOG_RX 5                  // PIn RX desky OpenLog
#define GPIO_OPENLOG_TX 6                  // PIn TX desky OpenLog


SoftwareSerial openlog(GPIO_OPENLOG_TX, GPIO_OPENLOG_RX);                                 // Obejkt softwarove seriove linky pro komunikaci s deskou OpenLog
RotaryEncoder enkoder(GPIO_ENKODER_1, GPIO_ENKODER_2, RotaryEncoder::LatchMode::TWO03);   // Objekt rotacniho enkoderu
Adafruit_SSD1306 displej(128, 64, &Wire, -1);                                             // Objekt OLED displeje
OneWire oneWire(GPIO_ONEWIRE);                                                            // Objekt sbernice OneWire
DallasTemperature teplomery(&oneWire);                                                    // Objekt teplomeru DS18B20 na sbernici OneWire

uint32_t cas = 0;                   // Cas posledni aktualizace teploty
uint32_t prodleva = 1000;           // Prodleva (ms) mezi aktualizacemi teploty
float teplota = 0;                  // Teplota
int termostat_cilova_teplota = 70;  // Vychozi cilova teplota automatickeho termostatu
bool termostat_zahriva = false;     // Stav zahrivani
bool termostat_aktivni = false;     // Stav termostatu

// Bitmapy ikon v dolni liste obrazovky
const unsigned char ico_chladnuti[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xf0, 0x1f, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xf8,
  0x3f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xfe,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
const unsigned char ico_zahrivani[] PROGMEM = {
  0x00, 0x10, 0x84, 0x00, 0x00, 0x18, 0xcc, 0x00, 0x00, 0x18, 0xc6, 0x00, 0x00, 0x18, 0xc6, 0x00,
  0x00, 0x18, 0xc6, 0x00, 0x00, 0x18, 0xcc, 0x00, 0x00, 0x31, 0x8c, 0x00, 0x00, 0x31, 0x8c, 0x00,
  0x00, 0x31, 0x88, 0x00, 0x0f, 0x31, 0x8c, 0xf0, 0x1f, 0x99, 0x8c, 0xf8, 0x1f, 0x80, 0x00, 0xf8,
  0x3f, 0xc2, 0x21, 0xfc, 0x3f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xfe,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
const unsigned char ico_vypnuto[] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x03,
  0xcf, 0xfe, 0x00, 0x03, 0xcf, 0xfe, 0x00, 0x03, 0xcf, 0xfe, 0x00, 0x03, 0xcf, 0xfe, 0x00, 0x03,
  0xcf, 0xfe, 0x00, 0x03, 0xcf, 0xfe, 0x00, 0x03, 0xcf, 0xfe, 0x00, 0x03, 0xcf, 0xfe, 0x00, 0x03,
  0xcf, 0xfe, 0x00, 0x03, 0xcf, 0xfe, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x03,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
const unsigned char ico_zapnuto[] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x03,
  0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x7f, 0xf3,
  0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x7f, 0xf3,
  0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x7f, 0xf3, 0xc0, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x03,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

//, Hlavni funkce setup se zpracuje hned na zacatku
void setup() {
  pinMode(GPIO_TLACITKO, INPUT_PULLUP);                // Nastaveni pinu tlacitka na vstup a interni pull-up cipu (v klidu stav HIGH)
  pinMode(GPIO_TOPNE_TELESO, OUTPUT);                  // Nastaveni pinu spinace na vystup
  digitalWrite(GPIO_TOPNE_TELESO, LOW);                // Vychozi stav spinace je LOW
  displej.begin(SSD1306_SWITCHCAPVCC, 0x3C);           // Nastartovani OLED displeje na I2C adrese 0x3C
  teplomery.begin();                                   // Start teplomeru DS18B20
  teplomery.setWaitForConversion(false);               // Relativne pomala komunikace s teplomerem nebude blokovat zbytek programu
  teplomery.requestTemperatures();                     // Povel ke zmereni teploty
  enkoder.setPosition(termostat_cilova_teplota);       // Vyvchozi stav rotacniho enkdoeru je roven vychozimu stavu cilove teploty (otacenim do stran ji zvysujeme/snizujeme)
  openlog.begin(9600);                                 // Nastartujeme seriove spojeni s deskou OpenLog rychlsoti 9600 b/s
}

// Nekonecna smycka loop se opakuje stale dokola
void loop() {
  detekujStisk(GPIO_TLACITKO, LOW, 50, &priStisku);    // Detekujeme stisk tlacitka (stav LOW) s 50ms debouncingem (odstraneni sumu pri stisku) a pripadne zavolame funkci priStisku
  if (millis() - cas > prodleva) {                     // Pokud uz ubehlo alespon [prodleva] milisekund
    teplomery.requestTemperatures();                   // Pozadame o teplotu
    teplota = teplomery.getTempCByIndex(0);            // Precteme teplotu jako cele cislo z prvniho teplomeru na sbernici OneWire
    openlog.println(teplota);                          // Ulozime teplotu na microSD pomoci modulu OpenLog
    teplomery.requestTemperatures();                   // Dame povel k dalsimu mereni
    kontrolaTermostatu();                              // Podle aktualni a cilove teploty sepneme/vypneme topne teleso
    prekresli();                                       // Prekreslime displej
    cas = millis();                                    // Aktualizujeme cas mereni
  }
  enkoder.tick();                                      // Detekujeme pootoceni enkoderu
  int32_t enkoder_pozice = enkoder.getPosition();      // Precteme citac enkoderu
  if (termostat_cilova_teplota != enkoder_pozice) {    // Pokud se citac zmenil, nastavime novou cilovou teplotu termostatu a prekreslime displej
    termostat_cilova_teplota = enkoder_pozice;
    prekresli();
  }
}

// Funkce termostatu, ktera podle nastavene cilove teploty spina, nebo vypina topne teleso
void kontrolaTermostatu() {
  if (termostat_aktivni) {
    if (teplota < termostat_cilova_teplota) {
      digitalWrite(GPIO_TOPNE_TELESO, HIGH);
      termostat_zahriva = true;
    } else {
      digitalWrite(GPIO_TOPNE_TELESO, LOW);
      termostat_zahriva = false;
    }
  }
}

// Funkce, ktera se zpracuje pri stisku tlacitka
// Pri stisku zapneme/vypneme funkci automatickeho termostatu
void priStisku() {
  termostat_aktivni = !termostat_aktivni;
  if (termostat_aktivni) {
    kontrolaTermostatu();
  } else {
    digitalWrite(GPIO_TOPNE_TELESO, LOW);
    termostat_zahriva = false;
  }
  prekresli();
}

// Funkce pro prekresleni displeje
void prekresli() {
  int16_t x1, y1; uint16_t w, h;                       // Pomocne promenne pro vypocet rozmeru textu na displeji
  char txt[10];                                        // Pamet pro textove retezce, ktere budeme vypisovat na displej
  displej.clearDisplay();                              // Smazeme snimek/framebuffer v RAM
  // NEJPRVE NAKRESLIME HLAVNI PANEL DISPLEJE                             
  displej.fillRect(0, 40, 128, 28, SSD1306_WHITE);     // Nakreslnime na dolni okraj bily panel 128x28
  if (termostat_zahriva) {                             // Pokud je topne teleso aktivni, nakreslime drobnou ikonku vyhrivane desky
    displej.drawBitmap(89, 42, ico_zahrivani, 32, 18, 0);
  } else {                                             // V opacnem pripade nakreslime ikonku studene desky
    displej.drawBitmap(89, 42, ico_chladnuti, 32, 18, 0);
  }
  if (termostat_aktivni) {                             // Pokud je zapnuty rezim automatickeho termostatu, nakreslime ikonku prepinace v zapnutem stavu
    displej.drawBitmap(47, 43, ico_zapnuto, 32, 18, 0);
  } else {                                             // Pokud je vypnuty rezim automatickeho termostatu, nakreslime ikonku prepinace ve vypnutem stavu
    displej.drawBitmap(47, 43, ico_vypnuto, 32, 18, 0);
  }
  itoa(termostat_cilova_teplota, txt, 10);             // Prevedeme cele cislo s cilovou teplotou na text
  displej.setTextColor(SSD1306_BLACK);                 // Nastavime barvu textu na cernou
  displej.setFont(&FreeMonoBold9pt7b);                 // Nastavime maly font
  displej.setCursor(3, 57);                            // Nastavime kurzor na pozici v levem dolnim rohu
  displej.print(txt);                                  // Nakreslime text
  // TED NAKRESLIME TEPLOTU
  displej.setTextColor(SSD1306_WHITE);                 // Nastavime barvu textu na bilou
  displej.setFont(&FreeMonoBold18pt7b);                // Nastavime velky font
  dtostrf(teplota, 3, 1, txt);                         // Prevedeme realne cislo s teplotou na text a presnosti na jedno desetinne misto
  displej.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);  // Spocitame rozmery textu s pouzitym fontem
  displej.setCursor((128 - w) / 2, 25);                // Podle rozmeru textu nastavime kurzor tak, aby se vypisoval doprostred displeje v prvni horni tretine
  displej.print(txt);                                  // Nakreslime text
  displej.display();                                   // Posleme snimek/framebuffer z RAM na displej
}

// Funkce pro detekci stisku tlacitka s podporou debouncingu
// Debouncing odstrani rozkmitani mechanickeho tlacitka pri stisku,
// kdy po prvni zmene logickeho stavu muze dojit jeste k nekolika dalsim
// po sobe jdoucim zmenam. My je smazeme cekanim, nez se nam signal na vstupu stabilizuje
// Dobu cekani v milisekundach urcuje promenna deboucing
// Promenna funkce je ukazatel na fiunkci, kterou po stisku zavolame 
void detekujStisk(uint8_t pin, uint8_t cil, uint16_t debouncing, void *funkce(void)) {
  int stav_pinu = digitalRead(pin);
  static int stav_tlacitka = HIGH;
  static uint8_t predchozi_stav_pinu = HIGH;
  static uint16_t cas_bouncingu = 0;
  if (stav_pinu != predchozi_stav_pinu) {
    cas_bouncingu = millis();
  }
  if (millis() - cas_bouncingu > debouncing) {
    if (stav_pinu != stav_tlacitka) {
      stav_tlacitka = stav_pinu;
      if (stav_tlacitka == cil) {
        funkce();
      }
    }
  }
  predchozi_stav_pinu = stav_pinu;
}
