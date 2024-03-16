// Zakladni demonstrace vyhrivani heratbed tile pro Original Prusa XL
// Pouzitiy hardware:
// - Heatbed tile
// - Klon Arduino Nano
// - 0.96" OLED, 128x64 px, monochormaticky, s radicem SSD1306
// - Teplomer DS18B20
// - Tlacitko
// - modul mosfetoveho spinace IRF520, rele atp.

// Knihovna pro sbernici I2C
#include <Wire.h>  
// Prace s grafikou, https://github.com/adafruit/Adafruit-GFX-Library// Prace s grafikou, https://github.com/adafruit/Adafruit-GFX-Library                    
#include <Adafruit_GFX.h>    
// Pouzity font, soucast Adafruit GFX          
#include <Fonts/FreeMonoBold24pt7b.h> 
// Prace s 0.96" OLED s radicem SSD1306, https://github.com/adafruit/Adafruit_SSD1306 
#include <Adafruit_SSD1306.h>          

#include <OneWire.h>            // Knihovna pro sbernici OneWire, https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>  // Knihovna pro praci s teplomerem DS18B20, https://github.com/milesburton/Arduino-Temperature-Control-Library

#define GPIO_ONEWIRE A1   // Pin sbernice OneWire
#define GPIO_SPINAC A2    // Pin mosfetoveho spinace napajeni heatbedu
#define GPIO_TLACITKO A3  // Pin tlacitka pro zapnuti/vypnuti vyhrivani

Adafruit_SSD1306 display(128, 64, &Wire, -1);  // OLED displej 128x64 px, na sbernici I2C
OneWire oneWire(GPIO_ONEWIRE);                 // Sbernice OneWire
DallasTemperature sensors(&oneWire);           // Teplomer DS18B20 na sbernici OneWire

uint32_t cas = 0;          // Cas posledni aktualizace teploty
uint32_t prodleva = 1000;  // Prodleva (ms)/perioda aktualizace teploty
int teplota = 0;           // Teplota
bool zahrivam = false;     // Stav zahrivani

// Bitmapa signalizace vyhrivani
const PROGMEM uint8_t ikona[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xc0, 0x81, 0x80, 0x01, 0xc1, 0x83, 0x00, 0x01, 0x83, 0x87, 0x00, 0x03, 0x87, 0x07, 0x00,
  0x03, 0x87, 0x0f, 0x00, 0x07, 0x87, 0x0f, 0x00, 0x07, 0x8f, 0x0f, 0x00, 0x07, 0x8f, 0x8f, 0x00,
  0x07, 0xcf, 0x8f, 0x00, 0x07, 0xc7, 0xcf, 0x80, 0x07, 0xe7, 0xcf, 0x80, 0x03, 0xe7, 0xef, 0xc0,
  0x03, 0xf7, 0xe7, 0xc0, 0x01, 0xf3, 0xe7, 0xe0, 0x01, 0xf3, 0xe3, 0xe0, 0x00, 0xf1, 0xf3, 0xe0,
  0x00, 0xf1, 0xf1, 0xe0, 0x00, 0xf1, 0xe1, 0xe0, 0x00, 0xf0, 0xe1, 0xe0, 0x00, 0xf0, 0xe1, 0xc0,
  0x00, 0xe1, 0xe1, 0xc0, 0x00, 0xe1, 0xc1, 0x80, 0x00, 0xc1, 0x83, 0x80, 0x01, 0x81, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//, Hlavni funkce setup se zpracuje hned na zacatku
void setup() {
  pinMode(GPIO_TLACITKO, INPUT_PULLUP);       // Nastaveni pinu tlacitka na vstup a interni pull-up cipu (v klidu stav HIGH)
  pinMode(GPIO_SPINAC, OUTPUT);               // Nastaveni pinu spinace na vystup
  digitalWrite(GPIO_SPINAC, LOW);             // Vychozi stav spinace je LOW
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // Nastartovani OLED displeje na I2C adrese 0x3C
  display.setFont(&FreeMonoBold24pt7b);       // Nastaveni rastrove pisma displeje
  display.setTextColor(SSD1306_WHITE);        // Nastaveni barvy pisma
  sensors.begin();                            // Start teplomeru DS18B20
  sensors.setWaitForConversion(false);        // Relativne pomala komunikace s teplomerem nebude blokovat zbytek programu
  sensors.requestTemperatures();              // Povel ke zmereni teploty
}

// Nekonecna smycka loop se opakuje stale dokola
void loop() {
  detekujStisk(GPIO_TLACITKO, LOW, 50);         // Detekujeme stisk tlacitka (stav LOW) s 50ms debouncingem
  if (millis() - cas > prodleva) {              // Pokud uz ubehlo alespon [prodleva] milisekund
    sensors.requestTemperatures();              // Pozadame o teplotu
    teplota = (int)sensors.getTempCByIndex(0);  // Precteme teplotu jako cele cislo z prvniho teplomeru na sbernici OneWire
    sensors.requestTemperatures();              // Dame povel k dalsimu mereni
    cas = millis();                             // Aktualizujeme cas mereni
    prekresli();                                // Prekreslime displej
  }
}

// Funkce, ktera se zpracuje pri stisku tlacitka
void priStisku() {
  zahrivam = !zahrivam;  // Prohodime hodnotu pravda/nepravda stavu zahrivani
  if (zahrivam) {
    digitalWrite(GPIO_SPINAC, HIGH);  // Pokud je stav zahrivani roven pravde, sepneme silove mosfetove napajeni
  } else {
    digitalWrite(GPIO_SPINAC, LOW);  // Pokud je stav zahrivani roven nepravde, vypneme silove mosfetove napajeni
  }
  prekresli();  // Prekreslime displej
}

// Funcke pro prekresleni displeje
void prekresli() {
  // Prevedeme teplotu na text a zjistime jeho rozmery
  // Pro vycentrovani na displej
  int16_t x1, y1;
  uint16_t w, h;
  char txt[10];
  sprintf(txt, "%d", teplota);
  display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (128 - w) / 2;
  int16_t y = 50;
  // Smazeme displej
  display.clearDisplay();
  // Pokud je stav zahrivani roven pravde, nakreslime na souradnice 5x5 bitmapu s ikonkou o rozmerech 32x32 px
  if (zahrivam) display.drawBitmap(5, 5, ikona, 32, 32, 1);
  // Nastavime kurzor na vypocitane souradnice pro centrovani textu
  display.setCursor(x, y);
  // Napiseme text s teplotou
  display.print(txt);
  // Posleme framebuffer skrze I2C do displeje
  display.display();
}

// Funkce pro detekci stisku tlacitka s podporou debouncingu
// Debounicng odstrani rozkmitani mechanickeho tlacitka pri stisku,
// kdy po prvni zmene logickeho stavu muze dojit jeste k nekolika dalsim
// po sobe jdoucim zmenam. My je smazeme cekanim, nez se nam signal na vstupu stabilizuje
// Dobu cekani v milisekundach urcuje promenna deboucing
void detekujStisk(uint8_t pin, uint8_t cil, uint16_t debouncing) {
  int stav = digitalRead(pin);
  static int stavTlacitka = HIGH;
  static uint8_t predchoziStav = HIGH;
  static uint8_t posledniDeboucning = 0;

  if (stav != predchoziStav) {
    posledniDeboucning = millis();
  }

  if ((millis() - posledniDeboucning) > debouncing) {
    if (stav != stavTlacitka) {
      stavTlacitka = stav;
      if (stavTlacitka == cil) {
        priStisku();
      }
    }
  }

  predchoziStav = stav;
}
