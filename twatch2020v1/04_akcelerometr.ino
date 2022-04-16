#define LILYGO_WATCH_2020_V1
#include <LilyGoWatch.h>

TTGOClass *hodinky;

void setup() {
  Serial.begin(115200);
  Serial.println("Startuji hodinky...");
  hodinky = TTGOClass::getWatch();
  hodinky->begin();
  delay(100);
  hodinky->openBL();
  hodinky->bl->adjust(255);
  hodinky->tft->fillScreen(TFT_ORANGE);
  hodinky->tft->setTextFont(4);
  hodinky->tft->setTextColor(TFT_BLACK, TFT_ORANGE);

  Acfg konfigurace;
  konfigurace.odr = BMA4_OUTPUT_DATA_RATE_25HZ;
  konfigurace.range = BMA4_ACCEL_RANGE_2G;
  konfigurace.bandwidth = BMA4_ACCEL_NORMAL_AVG4;
  konfigurace.perf_mode = BMA4_CONTINUOUS_MODE;
  hodinky->bma->accelConfig(konfigurace);
  hodinky->bma->enableAccel();
}

void loop() {
  static uint8_t rotace = 0;
  uint8_t novaRotace = hodinky->bma->direction();
  if (novaRotace != rotace) {
    rotace = novaRotace;
    hodinky->tft->fillScreen(TFT_ORANGE);
    switch (rotace) {
      case DIRECTION_DISP_DOWN:
      case DIRECTION_DISP_UP:
        hodinky->tft->drawString("VODOROVNE", 40, 100);
        break;
      case DIRECTION_BOTTOM_EDGE:
        hodinky->tft->drawString("NAHORU", 60, 100);
        break;
      case DIRECTION_TOP_EDGE:
        hodinky->tft->drawString("DOLU", 80, 100);
        break;
      case DIRECTION_RIGHT_EDGE:
        hodinky->tft->drawString("DOLEVA", 70, 100);
        break;
      case DIRECTION_LEFT_EDGE:
        hodinky->tft->drawString("DOPRAVA", 60, 100);
        break;
      default:
        break;
    }
  }
}
