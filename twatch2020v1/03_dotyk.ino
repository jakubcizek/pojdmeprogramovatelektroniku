#define LILYGO_WATCH_2020_V1
#include <LilyGoWatch.h>

TTGOClass *hodinky;
uint32_t aktualizaceProdleva = 1000;
uint32_t posledniAktualizace = 0;
uint16_t pozadi;
uint8_t barvaPozice = 0;
int16_t x, y;

uint16_t barvy[10] = {
  TFT_ORANGE, TFT_WHITE, TFT_BLUE,
  TFT_GREEN, TFT_RED, TFT_CYAN,
  TFT_PINK, TFT_VIOLET, TFT_PURPLE, TFT_NAVY
};

void nakresliCas() {
  RTC_Date cas = hodinky->rtc->getDateTime();
  char txt[9];
  snprintf(txt, sizeof(txt), "%02d:%02d:%02d",
           cas.hour, cas.minute, cas.second);
  hodinky->tft->setTextFont(7);
  hodinky->tft->setTextColor(TFT_BLACK, pozadi);
  hodinky->tft->drawString(txt, 10, 90);
}

void setup() {
  pozadi = barvy[0];
  Serial.begin(115200);
  Serial.println("Startuji hodinky...");
  hodinky = TTGOClass::getWatch();
  hodinky->begin();
  delay(100);
  hodinky->openBL();
  hodinky->bl->adjust(255);
  hodinky->rtc->setDateTime(2022, 3, 4, 15, 0, 0);
  hodinky->tft->fillScreen(pozadi);
  hodinky->motor_begin();
}

void loop() {
  if (millis() - posledniAktualizace > aktualizaceProdleva) {
    posledniAktualizace = millis();
    nakresliCas();
  }

  if (hodinky->getTouch(x, y)) {
    hodinky->motor->onec(50);
    Serial.println("Dotyk");
    pozadi = barvy[++barvaPozice];
    if (barvaPozice > 9) barvaPozice = 0;
    hodinky->tft->fillScreen(pozadi);
    char txt[20];
    snprintf(txt, sizeof(txt), "X=%d, Y=%d", x, y);
    hodinky->tft->setTextFont(4);
    hodinky->tft->setTextColor(TFT_BLACK, pozadi);
    hodinky->tft->drawString(txt, 10, 10);
    nakresliCas();
    delay(500);
  }
}
