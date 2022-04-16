#define LILYGO_WATCH_2020_V1
#include <LilyGoWatch.h>

TTGOClass *hodinky;
uint32_t aktualizaceProdleva = 1000;
uint32_t posledniAktualizace = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Startuji hodinky...");
  hodinky = TTGOClass::getWatch();
  hodinky->begin();
  delay(100);
  hodinky->openBL();
  hodinky->bl->adjust(255);
  hodinky->rtc->setDateTime(2022, 4, 17, 18, 45, 0);
  hodinky->tft->fillScreen(TFT_ORANGE);
  hodinky->tft->setTextColor(TFT_BLACK, TFT_ORANGE);
  hodinky->tft->setTextFont(7);
}

void loop() {
  if (millis() - posledniAktualizace > aktualizaceProdleva) {
    posledniAktualizace = millis();
    RTC_Date cas = hodinky->rtc->getDateTime();
    char txt[9];
    snprintf(txt, sizeof(txt), "%02d:%02d:%02d", cas.hour, cas.minute, cas.second);
    hodinky->tft->drawString(txt, 10, 90);
  }
}
