#include <Arduino_HTS221.h> // Pouzijeme knihovnu pro cip teplomeru HTS221

// Hlavni funkce setup se zpracuje hned na zacatku
void setup() {
  Serial.begin(115200); // Nastartujeme seriovou linku na USB
  while (!Serial); // Pockame, dokud v PC neotevreme seriovy monitor
  HTS.begin(); // Nastartujeme teplomer
}

// Nekonecna smycka loop se spusti po zpracovani funkce setup
void loop() {
  float teplota = HTS.readTemperature(); // Zmer teplotu
  float vlhkost = HTS.readHumidity(); // Zmer vlhkost
  Serial.print(teplota); Serial.println(" °C"); // Vypis teplotu do seriove linky
  Serial.print(vlhkost); Serial.println(" %"); // Vypis vlhkost do seriove linky
  delay(1000); // Pockej 1 000 ms a vse opakuj
}
