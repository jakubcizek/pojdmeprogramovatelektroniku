// Knihovna pro ovladnuti kamery PMW3901
// https://github.com/bitcraze/Bitcraze_PMW3901
#include "Bitcraze_PMW3901.h"

// Objekt kamery PMW3901
// Kamera musi byt pripojena na piny sbernice SPI
// Parametrem je pin CS,
// ktery jsem v pripade desky XIAO ESP32C3 pripojil na jeji pin D3
Bitcraze_PMW3901 kamera(D3);

// Hlavni funkce setup se spusti hned po startu
void setup() {
  // Nastartujeme seriovou linku do PC co nejvyssi rychlosti
  // Zalezi na pouzitem cipu
  Serial.begin(500000);
  // Nastartujeme kameru
  kamera.begin();
}

// Funkce loop predstavuje nekonecnou smycku programu
void loop() {
  // Pomocne promenne pro relativny zmenu X a Y od posledniho mereni
  int16_t deltaX, deltaY;
  // Precteme hodnty zmen v osach X a Y
  kamera.readMotionCount(&deltaX, &deltaY);
  // Vypiseme je do seriove linky oddelene mezerou
  Serial.print(deltaX);
  Serial.print(" ");
  Serial.print(deltaY);
  Serial.print("\n");
  // Pockame 100 ms, abychom dokazali zachytit dostatecne velkou relativni zmenu
  // a vse zopakujeme
  delay(100);
}
