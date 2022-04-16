#define LILYGO_WATCH_2020_V1
#include <LilyGoWatch.h>

TTGOClass *hodinky;

void setup() {
  Serial.begin(115200);
  Serial.println("Startuji hodinky...");
  hodinky = TTGOClass::getWatch();
  hodinky->begin();
  hodinky->openBL();
  delay(100);
  hodinky->bl->adjust(255);

  hodinky->tft->fillScreen(TFT_ORANGE);
  hodinky->tft->drawRect(19, 59, 202, 102 ,TFT_BLACK);
  hodinky->tft->fillRect(20, 60, 200, 100 ,TFT_GREEN);
  hodinky->tft->setTextFont(4);
  hodinky->tft->setTextColor(TFT_RED);
  hodinky->tft->drawString("MOJE", 85, 80);
  hodinky->tft->setTextColor(TFT_BLUE);
  hodinky->tft->drawString("HODINKY", 65, 120);
}

void loop() {;}
