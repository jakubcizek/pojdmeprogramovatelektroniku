// Vsechny tyto knihovny jsou soucasti podpory pro cipy ESP32 v Arduinu:
// https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
#include <esp_camera.h>  // Knihovna pro praci s vybranymi kamerami
#include <FS.h>          // Knihovna pro praci se souborovym systemem
#include <SD.h>          // Knihovna pro praci s SD kartou
#include <SPI.h>         // Knihovna pro praci se sbernici SPI (SD karta)

// Digitalni piny, ktere na desce XIAO ESP32S3 Sense
// okupuje kamera pro 8bit paralelni komunikaci
#define PWDN_GPIO -1
#define RESET_GPIO -1
#define XCLK_GPIO 10
#define SIOD_GPIO 40
#define SIOC_GPIO 39
#define DATA0_GPIO 15
#define DATA1_GPIO 17
#define DATA2_GPIO 18
#define DATA3_GPIO 16
#define DATA4_GPIO 14
#define DATA5_GPIO 12
#define DATA6_GPIO 11
#define DATA7_GPIO 48
#define VSYNC_GPIO 38
#define HREF_GPIO 47
#define PCLK_GPIO 13
#define LED_GPIO 21

// Digitalni pin CS/SS pro SPI komunikaci s microSD kartou
#define SD_SS 21

uint16_t pocitadlo = 0;   // Pocitadlo fotek
bool kamera_ok = false;   // Pomocna promenna stavu inicializace kamery
bool microsd_ok = false;  // Pomocna kamera stavu inicializace SD karty

// Funkce pro sejmuti a ulozeni fotky na microSD
void ulozitFotku(const char *cesta) {
  // Ziskej pole bajtu (frame buffer) fotky
  // Bajty uz maji format JPEG obrazku
  Serial.print("Ziskavam fotku... ");
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    Serial.println("OK");
  } else {
    Serial.println("CHYBA");
    return;
  }
  // Ulozime bajty do souboru na microSD
  ulozitSoubor(SD, cesta, fb->buf, fb->len);
  // Uvolnime frame buffer opet pro potreby kamery
  esp_camera_fb_return(fb);
}

// Funkce pro zapsani souboru na microSD
void ulozitSoubor(fs::FS &fs, const char *cesta, uint8_t *data, size_t delka) {
  Serial.printf("Ukladam soubor %s... ", cesta);
  File soubor = fs.open(cesta, FILE_WRITE);
  // Pokud nedokazeme otevrit/vyt vytvorit soubor
  if (!soubor) {
    Serial.println("CHYBA OTEVRENI");
    return;
  }
  // Pokud jsme zapsali vsechny bajty do souboru
  if (soubor.write(data, delka) == delka) {
    Serial.println("OK");
  } else {
    Serial.println("CHYBA ZAPISU");
  }
  soubor.close();
}

// Hlavni funkce setup se spusti po startu
void setup() {
  // Nastartujeme seriovou linku do PC
  // a cekame, dokud se nepripoji klient
  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("Start programu");

  // Abychom mohli pracovat s 2MPx fotkou v RAM, musi byt dodstatecne velka.
  // XAIO ESP32S3 je proto vyzbrojena externi 8MB PSRAM
  // Je treba ji aktivovat pred flashem v Arduinu v menu Nastroje/Tools
  // Na zacatku programu jen overime, ze je opravdu k dispozici
  if (psramInit()) {
    Serial.println("Vypada to, ze PSRAM funguje");
  } else {
    Serial.println("PSRAM neni k dispozici :-(. ZKontroluj nastaveni v Arduino IDE Tools/Nastroje");
  }
  // Pro kontrolu vypiseme velikost volne interni a externi pameti RAM
  Serial.printf("Dostupny interni heap %d B z %d B\n", ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("Dostupna externi PSRAM %d B z %d B\n", ESP.getFreePsram(), ESP.getPsramSize());

  // Konfigurace kamery
  // Nastavime vsechny digitalni piny, ktere pouziva
  // Ale take rozliseni na UXGA (1600x1200)
  // A format vystupniho frame bufferu na JPEG
  // UXGA ale muzeme pouzit jen s velkou RAM,
  // a proto nastavime lokaci frame bufferu na externi PSRAM
  // Popis dalsich parametru prostudujte zde:
  // https://components.espressif.com/components/espressif/esp32-camera
  // https://github.com/espressif/esp32-camera
  // https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h
  camera_config_t konfigurace_kamery;
  konfigurace_kamery.ledc_channel = LEDC_CHANNEL_0;
  konfigurace_kamery.ledc_timer = LEDC_TIMER_0;
  konfigurace_kamery.pin_d0 = DATA0_GPIO;
  konfigurace_kamery.pin_d1 = DATA1_GPIO;
  konfigurace_kamery.pin_d2 = DATA2_GPIO;
  konfigurace_kamery.pin_d3 = DATA3_GPIO;
  konfigurace_kamery.pin_d4 = DATA4_GPIO;
  konfigurace_kamery.pin_d5 = DATA5_GPIO;
  konfigurace_kamery.pin_d6 = DATA6_GPIO;
  konfigurace_kamery.pin_d7 = DATA7_GPIO;
  konfigurace_kamery.pin_xclk = XCLK_GPIO;
  konfigurace_kamery.pin_pclk = PCLK_GPIO;
  konfigurace_kamery.pin_vsync = VSYNC_GPIO;
  konfigurace_kamery.pin_href = HREF_GPIO;
  konfigurace_kamery.pin_sscb_sda = SIOD_GPIO;
  konfigurace_kamery.pin_sscb_scl = SIOC_GPIO;
  konfigurace_kamery.pin_pwdn = PWDN_GPIO;
  konfigurace_kamery.pin_reset = RESET_GPIO;
  konfigurace_kamery.xclk_freq_hz = 20000000;
  konfigurace_kamery.frame_size = FRAMESIZE_UXGA;
  konfigurace_kamery.pixel_format = PIXFORMAT_JPEG;  // JPEG
  konfigurace_kamery.grab_mode = CAMERA_GRAB_LATEST;
  konfigurace_kamery.fb_location = CAMERA_FB_IN_PSRAM;  // Frame buffer v PSRAM
  konfigurace_kamery.jpeg_quality = 10;                 // JPEG kvalita
  // Diky tomu, ze jsme alokovali pamet na dva snimky a grab_mode na CAMERA_GRAB_LATEST
  // kamera ji bude cyklicky zaplnovat, stale bezi a ma nizkou latenci
  // Pokud snizime na 1, prodleva snimku se snizi, ale kamera zase nebude mit takovou spotrebu
  // Grab_mode bychom pak mohli nastavit jeste na CAMERA_GRAB_WHEN_EMPTY
  konfigurace_kamery.fb_count = 2;  // Pocet framu v pameti

  // Nastavime knihovnu pro praci s kamerou pomoci nasi konfigurace,
  // nebo vratime chybu
  esp_err_t chyba = esp_camera_init(&konfigurace_kamery);
  if (chyba != ESP_OK) {
    Serial.printf("Nemohu nastartovat kameru: %d (0x%x)", chyba, chyba);
    return;
  }
  // Pomocna promenna signalizujici, ze kamera je pripravena
  kamera_ok = true;

  // Nastartujeme SD kartu
  if (!SD.begin(SD_SS)) {
    Serial.println("Nemohu nastartovat microSD :-(");
    return;
  }
  uint8_t typ = SD.cardType();
  if (typ == CARD_NONE) {
    Serial.println("Neni pripojena zadna karta :-(");
    return;
  }

  Serial.print("microSD karta: ");
  if (typ == CARD_MMC) {
    Serial.println("MMC");
  } else if (typ == CARD_SD) {
    Serial.println("SD");
  } else if (typ == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("Neznama");
  }
  // Pomocna promenna signalizujici, ze SD karta je pripravena
  microsd_ok = true;
}

// Smycka loop se opakuje stale dokola
void loop() {
  // Pokud dorazila nejaka data po seriove lince
  while (Serial.available()) {
    // Precti pripadny text az po zalomeni radku
    String prikaz = Serial.readStringUntil('\n');
    // Pokud text zacina slovy "FOTO" a kamera a SD jsou pripravene
    if (prikaz.startsWith("FOTO") && kamera_ok && microsd_ok) {
      // Vytvorime nazev souboru obrazku ve formatu /obrazek_XXXXX.jpg
      char nazev_souboru[19];
      sprintf(nazev_souboru, "/OBRAZEK_%05d.JPG", ++pocitadlo);
      ulozitFotku(nazev_souboru);
    }
  }
}