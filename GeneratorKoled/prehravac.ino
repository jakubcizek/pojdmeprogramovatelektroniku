#include "SilentNight.mid.h"
void setup() {
    ;
}

void loop() {
    for (uint16_t i = 0; i < delkaKoledy; i++) {
        uint16_t ton = pgm_read_word(&(koleda[i][0]));
        int16_t delka = pgm_read_word(&(koleda[i][1]));
        if(ton > 0) tone(3, ton);
        else noTone(3);
        delay(delka);
    }
    noTone(3);
    delay(1000);
}