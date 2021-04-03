/*
 * Demoukazka oziveni prstence s 16 RGB LED s cipy WS2812B
 * Pouzite knihovny:
 * Adafuit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
 * Pouzita deska: NodeMCU v3 s cipem ESP8266
*/

#include <Adafruit_NeoPixel.h>

/*
 * Parametry tridy prstenec
 * Pocet LED: 16
 * GPIO pin: D1
 * Poradi pixelu + rychlost komunikace: GRB + 800 kHz
*/
Adafruit_NeoPixel prstenec(16, D1, NEO_GRB + NEO_KHZ800);

// Hlavni funkce setup se spousti hned po startu cipu
void setup(){
    prstenec.begin(); // Priprav prstenec
    prstenec.clear(); // Vypni vsechny LED
    prstenec.setBrightness(5); // Nastav jas LED na maximum (0-255)
    prstenec.setPixelColor(0, prstenec.Color(255, 0, 0)); // Rozsvit 0. LED plnym cervenym kanalem 
    prstenec.setPixelColor(8, prstenec.Color(0, 255, 0)); // Rozsvit 7. LED plnym zeleym kanalem
    /* Vsechny upravy jsme doposud provedli jen v pameti mikrokontroleru.
     * Teprve nyni rekneme knihovne, at podle nasi konfigurace vytvori signal, 
     * ktery nastavi cipy WS2812 v jednotlivych LED prstence
    */
    prstenec.show();

}

// Smycka loop se opakuje stale dokola a spousti se p ozpracovani funkce setup
void loop(){}
