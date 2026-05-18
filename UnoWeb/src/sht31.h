#pragma once
#include <Arduino.h>
#include "config.h"

// Sensirion SHT31 (SHT3x) pres primy TWI (I2C). Adresa 0x44.
// Single-shot, high repeatability, bez clock-stretchingu (prikaz 0x2400).
void sht31_init();

// Pri uspechu naplni t100 (°C x100) a rh100 (% x100) a vrati true.
// Pri TWI nebo CRC chybe vrati false (volajici ponecha posledni hodnotu).
bool sht31_read(int16_t* t100, int16_t* rh100);

#ifdef APP_DEBUG
// Diagnosticky I2C sken (jen v debug rezimu). Vyzaduje sht31_init() pred.
void i2c_scan();
#endif
