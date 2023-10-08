// Knihovna pro ovladnuti kamery PMW3901
// https://github.com/bitcraze/Bitcraze_PMW3901
#include <Bitcraze_PMW3901.h>

// Objekt kamery PMW3901
// Kamera musi byt pripojena na piny sbernice SPI
// Parametrem je pin CS,
// ktery jsem v pripade desky XIAO ESP32C3 pripojil na jeji pin D3
Bitcraze_PMW3901 kamera(D3);

// Pole 1225 bajtu pro 35x35px snimek z kamery
uint8_t snimek[35 * 35];

// Hlavni funkce setup se spusti hned po startu
void setup() {
  // Nastartujeme seriovou linku do PC co nejvyssi rychlosti
  // Zalezi na pouzitem cipu
  Serial.begin(500000);
  // Nastartujeme kameru a prepneme ji do rezimu ukladani celych snimku
  kamera.begin();
  kamera.enableFrameBuffer();
}

// Funkce loop predstavuje nekonecnou smycku programu
void loop() {
  // Precteme cely snimek z kamery do pole snimek
  // Muze to trvat az nekolik dlouhych sekund
  kamera.readFrameBuffer((char *)snimek);
  int x, y, pozice;
  // Prochazime snimek po radcich
  for (y = 0, pozice = 0; y < 35; y++) {
    // Na zacatku kazdeho radku odesleme do seriove linky jeho cislo a mezeru,
    // aby druha strana vedela, jaky radek snimku zrovna dorazil
    // Takze treba "0 ". Data posilame pro nazornost jako prosty text,
    // binarne by to bylo rychlejsi
    Serial.print(y);
    Serial.print(" ");
    // Ted vypiseme aktualni radek jako radu cisel oddelenych mezerou
    // Kazde cislo predstavuje barvu pixelu jako 8bit odstin sedi (0-255)
    for (x = 0; x < 35; x++, pozice++) {
      Serial.print(snimek[pozice]);
      Serial.print(" ");
    }
    // Zalomime radek a pokracujeme
    // Cely radek tedy predstavuje 35+1 hodnot, pricemz ta prvni je cislo radku
    // Druha strana totiz potrebuje vedet, kde zacina novy snimek (dorazil radek 0)
    Serial.println();
  }
}
