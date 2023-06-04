#include <SPI.h>

// Ukazatel na pametu pro bitmapu
uint8_t* bitmap_buffer;

// GPIO piny pro pomocne signaly displeje
#define BUSY_Pin 27
#define RES_Pin 26
#define DC_Pin 25
#define CS_Pin 5

// Velikost pameti pro bitmapu
#define BITMAP_SIZE (800 * 480) / 2

// Makra pro nastaveni digitalnich stavu na pomocnych pinech
#define DIGITALNI_STAV_CS_0 digitalWrite(CS_Pin, LOW)
#define DIGITALNI_STAV_CS_1 digitalWrite(CS_Pin, HIGH)
#define DIGITALNI_STAV_DC_0 digitalWrite(DC_Pin, LOW)
#define DIGITALNI_STAV_DC_1 digitalWrite(DC_Pin, HIGH)
#define DIGITALNI_STAV_RST_0 digitalWrite(RES_Pin, LOW)
#define DIGITALNI_STAV_RST_1 digitalWrite(RES_Pin, HIGH)
#define BUSY_STAV digitalRead(BUSY_Pin)

//4bit
#define black 0x00   /// 000
#define white 0x01   /// 001
#define green 0x02   /// 010
#define blue 0x03    /// 011
#define red 0x04     /// 100
#define yellow 0x05  /// 101
#define orange 0x06  /// 110
#define clean 0x07   /// 111

// Makra s ridicimi instrukcemi e-inku
#define PSR 0x00
#define PWRR 0x01
#define POF 0x02
#define POFS 0x03
#define PON 0x04
#define BTST1 0x05
#define BTST2 0x06
#define DSLP 0x07
#define BTST3 0x08
#define DTM 0x10
#define DRF 0x12
#define PLL 0x30
#define CDI 0x50
#define TCON 0x60
#define TRES 0x61
#define REV 0x70
#define VDCS 0x82
#define T_VDCS 0x84
#define PWS 0xE3

// Pouzite funkce
void eink_data(uint8_t prikaz);
void eink_cmd(uint8_t data);
void eink_init(void);
void eink_spanek(void);
void eink_cekej_na_vykresleni(void);
void eink_bitmapa(void);
void bitmap_psram_init(void);


// Vytvoreni pameti pro lokalni frame buffer
// Vytvarime ho v externi PSRAM SoC modulu ESP32-WROVER-E

// ps_malloc JE TREBA ZMENIT za malloc, POKUD POUZIVATE JINY CIP !!!! 

// Anebo upravit kod takovym zpusobem, aby se pamet vubec nealokovala,
// pokud na ni neni v RAM misto. Treba tak, ze budou bajty odesilat do
// frame bufferu displeje uz pri stahovani. Viz funcke stahni_a_dekomprimuj
void bitmap_psram_init(void){
  bitmap_buffer = (uint8_t*)ps_malloc(BITMAP_SIZE);
}

// Funkce pro oodeslani prikazu do e-inku
void eink_cmd(uint8_t prikaz) {
  DIGITALNI_STAV_CS_0;
  DIGITALNI_STAV_DC_0;
  SPI.transfer(prikaz);
  DIGITALNI_STAV_CS_1;
}

// Funkce pro odeslani dat do e-inku
void eink_data(uint8_t data) {
  DIGITALNI_STAV_CS_0;
  DIGITALNI_STAV_DC_1;
  SPI.transfer(data);
  DIGITALNI_STAV_CS_1;
}

// Funkce pro inicializaci displeje
void eink_init(void) {
  DIGITALNI_STAV_RST_0; // Reset
  delay(10); // Dle dokumentace cekat alespon 10 ms
  DIGITALNI_STAV_RST_1;
  delay(10); // Dle dokumentace cekat alespon 10 ms

  eink_cmd(0xAA); // CMDH
  eink_data(0x49);
  eink_data(0x55);
  eink_data(0x20);
  eink_data(0x08);
  eink_data(0x09);
  eink_data(0x18);

  eink_cmd(PWRR);
  eink_data(0x3F);
  eink_data(0x00);
  eink_data(0x32);
  eink_data(0x2A);
  eink_data(0x0E);
  eink_data(0x2A);

  eink_cmd(PSR);
  eink_data(0x5F);
  eink_data(0x69);

  eink_cmd(POFS);
  eink_data(0x00);
  eink_data(0x54);
  eink_data(0x00);
  eink_data(0x44);

  eink_cmd(BTST1);
  eink_data(0x40);
  eink_data(0x1F);
  eink_data(0x1F);
  eink_data(0x2C);

  eink_cmd(BTST2);
  eink_data(0x6F);
  eink_data(0x1F);
  eink_data(0x16);
  eink_data(0x25);

  eink_cmd(BTST3);
  eink_data(0x6F);
  eink_data(0x1F);
  eink_data(0x1F);
  eink_data(0x22);

  eink_cmd(0x13); // IPC
  eink_data(0x00);
  eink_data(0x04);

  eink_cmd(PLL);
  eink_data(0x02);

  eink_cmd(0x41); // TSE
  eink_data(0x00);

  eink_cmd(CDI);
  eink_data(0x3F);

  eink_cmd(TCON);
  eink_data(0x02);
  eink_data(0x00);

  eink_cmd(TRES);
  eink_data(0x03);
  eink_data(0x20);
  eink_data(0x01);
  eink_data(0xE0);

  eink_cmd(VDCS);
  eink_data(0x1E);

  eink_cmd(T_VDCS);
  eink_data(0x00);

  eink_cmd(0x86); // AGID
  eink_data(0x00);

  eink_cmd(PWS);
  eink_data(0x2F);

  eink_cmd(0xE0); // CCSET
  eink_data(0x00);

  eink_cmd(0xE6); // TSSET
  eink_data(0x00);

  eink_cmd(0x04); //power on
  eink_cekej_na_vykresleni(); 
}

// Prechod displeje do spanku
void eink_spanek(void) {
  eink_cmd(0X02);
  eink_data(0x00);
  eink_cekej_na_vykresleni();
}

// Funkce pro kresleni bitmapy
void eink_bitmapa(void) {
  // Zapisujeme pixely do frame bufferud displeje
  eink_cmd(0x10);
  for (uint32_t bajt = 0; bajt < BITMAP_SIZE; bajt++) {
    eink_data(bitmap_buffer[bajt]);
  }
  // Povel k refreshi displeje
  eink_cmd(0x12);
  eink_data(0x00);
  delay(1);  
  eink_cekej_na_vykresleni();
}

// Blokujici funkce, dokud se na pinu BUSY nezmeni stav indikujici, ze prekreslovani je hotove
void eink_cekej_na_vykresleni(void) {
  while (!BUSY_STAV)
    ;  //0:BUSY, 1:VOLNO
}
