/*
 * Minimalistický driver monochromatického OLED 128x64 přes I2C
 * (nová I2C master API, ESP-IDF 5.x). Kreslí text 8x8 fontem se škálováním.
 *
 * Podporované řadiče (volba v menuconfig -> Casomira -> Radic OLED displeje):
 *   - SSD1306 (typicky 0.96" moduly)
 *   - SH1106  (typicky 1.3" moduly, napr. DIYMORE) - zapis s offsetem 2 sloupcu
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Inicializuje I2C i displej. i2c_addr je 7bitová adresa (0x3C pro modul "0x78").
// Vrátí false, pokud displej na sběrnici neodpoví (program pak běží bez OLED).
bool ssd1306_init(int sda_gpio, int scl_gpio, uint8_t i2c_addr);

// Smaže interní framebuffer (neprojeví se, dokud nezavoláš ssd1306_flush).
void ssd1306_clear(void);

// Šířka textu v pixelech pro dané měřítko (scale 1..3).
int ssd1306_text_width(const char *s, int scale);

// Vykreslí text; x,y jsou pixely levého horního rohu, scale 1..3.
void ssd1306_text(int x, int y, const char *s, int scale);

// Vykreslí text vodorovně na střed na svislé pozici y (pixely).
void ssd1306_text_centered(int y, const char *s, int scale);

// Velký proporcionální font (24 px) jen pro čas: znaky "0123456789:.".
// Jiné znaky se přeskočí. Určeno pro výsledný čas přes celý displej.
int  ssd1306_bigtext_width(const char *s);
void ssd1306_bigtext(int x, int y, const char *s);
void ssd1306_bigtext_centered(int y, const char *s);

// Odešle framebuffer na displej.
void ssd1306_flush(void);

// Ladicí I2C sken: vypíše do logu adresy zařízení na sběrnici. Volá se
// automaticky, když se OLED nenajde; lze zavolat i ručně (po ssd1306_init).
void ssd1306_i2c_scan(void);
