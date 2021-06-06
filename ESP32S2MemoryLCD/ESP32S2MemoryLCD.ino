#include <Adafruit_GFX.h> // Knihovna pro pokrocile graficke operace, https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SharpMem.h> // Knihovna pro praci s Memory LCD od Sharpu, https://github.com/adafruit/Adafruit_SHARP_Memory_Display
#include "Bungee_Regular30pt7b.h" // Rasterizovane pismo Bungee pomoci nastroje truetype2gfx, https://rop.nl/truetype2gfx/
#include "logo.h" // Bitmapa loga Zive.cz pomoci nastroje image2cpp, https://javl.github.io/image2cpp/

#define SHARP_SCK  36 // Pin SCK sbernice SPI (zalezi na pouzite desce/cipu)
#define SHARP_MOSI 35 // Pin MOSI sbernice SPI (zalezi na pouzite desce/cipu)
#define SHARP_SS   34 // Pin SS sbernice SPI (zalezi na pouzite desce/cipu)
#define SHARP_5V_ON 42 // Pomocny pin konkretni desky od Lubose Moravce (@bastlir), ktery aktivuje 5V napajeni displeje

#define SIRKA 400 // Sirka displeje
#define VYSKA 240 // Vyska displeje
#define CERNA 0 // Cerna barva
#define BILA 1 // Bila barva

Adafruit_SharpMem displej(SHARP_SCK, SHARP_MOSI, SHARP_SS, SIRKA, VYSKA); // Objekt displeje pripojneho na sbernici SPI

// Pomocna funkce pro vypsani textu doprostred radku displeje
void napisDoprostred(const String &txt, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  displej.getTextBounds(txt, 0, y, &x1, &y1, &w, &h); // Metoda vypocita rozmery obdelniku textu na displeji
  displej.setCursor((SIRKA / 2) - (w / 2), y); // Pomoci zname sirky displeje a sirky textu vypocitame souradnice pro jeho umisteni do stredu
  displej.print(txt); // Napis text
}

// Hlavni funkce setup se zpracuje ihned po propojeni rididcho cipu/desky k napajeni 
void setup() {
  pinMode(SHARP_5V_ON, OUTPUT); // Aktivuj 5V napajeni LCD (pouze u nasi desky)
  digitalWrite(SHARP_5V_ON, HIGH); // Aktivuj 5V napajeni LCD (pouze u nasi desky)
  displej.begin(); // Nastartuj displej
  displej.clearDisplay(); // Resetuj jeho 12kB frame buffer v RAM naseho mikrokontroleru
  displej.setRotation(0); // Nastav orintaci displeje na vychozi sirokouhly pohled
  displej.setFont(&Bungee_Regular30pt7b); // Nastav rastrove pismo z hlavickoveho souboru
  displej.setTextColor(CERNA); // Nastav barvu pisma na cernou
  napisDoprostred("Mame radi", 70); // Na vysku 10 px napis na stred prvni radek
  displej.drawBitmap(100, 100, logo, 200, 110, CERNA); // Na souradnice 0x46 vykresli cernou barvou pole logo s bitmapou o rozmerech 400x194 z hlavickoveho souboru 
  displej.refresh(); // Konecne posli frame buffer radek po radku skrze sbernici SPI do displeje
}

// Obsah smycky loop se opakuje stale dokola a funkce nastartuje po zpracovani funkce setup
// Tady bychom mohli periodicky cokoliv vypisovat na displej atp.
void loop() {
}
