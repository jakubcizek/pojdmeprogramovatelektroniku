#include <WiFi.h> // Pripojeni k Wi-Fi
#include <HTTPClient.h> // HTTP klient; stazeni bitmapy
#include "gdey073d46.h" // Hlavickovy soubor s funkcemi pro praci s e-inkem

// Nazev a heslo Wi-Fi
const char* ssid = "Nazev 2.4GHZ 802.1bgn Wi-Fi";
const char* heslo = "WPA heslo k Wi-Fi";

// URL bitmapy pro eink
// Soubor vytvori generator v Pythonu, musite jej ale spoustet na vlastnim serveru
String url = "http:// ???? /dashboard_rle.bin";

RTC_DATA_ATTR uint32_t pocitadlo_probuzeni = 0; // Pocitadlo probuzeni (vypisuje se pro kontrolu do seriove linky)
uint64_t prodleva_s = 3600; // Prodleva v sekundach mezi probuzenim hlavniho procesoru z hlubokeho spanku (1 hodina)

// Funkce pro stazeni a dekomprimaci bitmapy do RAM
uint32_t stahni_a_dekomprimuj() {
  uint32_t stazeno = 0;
  HTTPClient http;
  http.setTimeout(10000); // Zvysimu timeout pro TCP spojeni na 10 sekund, pokud budeme bitmapu generovat pri HTTP pozadavku (muze byt velka prodleva, nez server zacne posilat data)
  http.begin(url); // Vytvorime HTTP pozadavek s nasi URL
  int kod = http.GET(); // Zaciname HTTP GET komunikaci
  Serial.printf(" (HTTP kod: %d) ", kod);
  if (kod == HTTP_CODE_OK) { // Pokud jsme navazali HTTP spojeni (odpoved HTTP OK)
    uint32_t velikost_soubor = http.getSize(); // Velikost dat ke stazeni
    Serial.printf(" (Ke stazeni: %d) ", velikost_soubor);
    WiFiClient* stream = http.getStreamPtr(); // Otevreme stream na stahovana data
    uint32_t dekomprimovane_bajty = 0;
    while (http.connected() && (stazeno < velikost_soubor)) { // Pokud jsme spojeni se serverem a jeste jsme nestahli cely soubor
      size_t k_dispozici = stream->available(); // Zjistime velikost aktualne stazenych dat ve streamovacim zasobniku
      if (k_dispozici) { // Pokud tam neco je
        uint8_t buffer[128];
        size_t precteno = stream->readBytes(buffer, 128); // Precteme 128 bajtu do naseho vlastniho maleho zasobniku (je to rychlejsi, nez cist bajt po bajtu)
        if (precteno) {
          // RLE dekomprese davky v zasobniku
          // !!! V nasem zasobniku musi byt sudy pocet bajtu, jinak mame problem !!!
          for (size_t pozice = 0; pozice < precteno; pozice += 2) { // Precteme vzdy dva bajty z naseho zasobniku. Prvni bajt je pocet, druhy bajt hodnota
            uint8_t pocet = buffer[pozice];
            uint8_t hodnota = buffer[pozice + 1];
            for (uint8_t pozice = 0; pozice < pocet; pozice++) bitmap_buffer[dekomprimovane_bajty++] = hodnota; // Vyplnime bitmapu davkou
          }
          stazeno += precteno; // Navysime pocitadlo stazenych bajtu
        }
      }
      delay(1); // Drobna cekaci prodleva pro zvyseni stability prenosu. Lze s tim experimentovat
    }
  }
  http.end(); // Ukonceni HTTP spojeni
  return stazeno; // Vratime pocet stazenych bajtu pro kontrolu
}


// Hlavni funkce setup se spusti hned na zacatku
void setup() {
  Serial.begin(115200); // Nastartujeme seriovou linku
  delay(1000); // Jen umela prodleva, abychom stacili otevrit seriovy terminal, pokud chceme
  // Konfigurace pomocnych signalu e-inku
  pinMode(BUSY_Pin, INPUT);
  pinMode(RES_Pin, OUTPUT);
  pinMode(DC_Pin, OUTPUT);
  pinMode(CS_Pin, OUTPUT);
  // Nastartovani sbernice SPI
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  SPI.begin();
  // Vytvoreni bloku pameti pro bitmapu
  // Pozor, blok pameti se alokuje v externi PSRAM
  // Pokud nemate desku s modulem ESP32-WROVER, je treba kosmeticky upravit podobu funkce bitmap_psram_init (nahradit ps_malloc za standardni malloc),
  // anebo vetsi merou prepsat kod tak, aby se nevytvarel lokalni frame buffer v RAM pro bitmapu
  Serial.printf("Alokuji %d bajtu v PSRAM pro 4bit bitmapu... ", BITMAP_SIZE);
  bitmap_psram_init();
  Serial.println("OK");
  // Pripojeni k Wi-Fi
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, heslo);
  Serial.printf("Pripojuji se k %s... ", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Vypiseme do seriove linky pro kontrolu LAN IP adresu obdrzenou od DHCP a hodnotu pocitadla probuzeni
  Serial.printf(" OK\r\nIP: %s\r\nPocet probuzeni: %d\r\n", WiFi.localIP().toString(), pocitadlo_probuzeni++);

  // Stahneme a dekomprimujeme bitmapu z naseho serveru
  Serial.print("Stahuji a dekomprimuji... ");
  uint32_t stazeno = stahni_a_dekomprimuj();
  Serial.printf("%d B\r\n", stazeno);
  Serial.println("Inicializuji e-ink...");

  // Inicializujeme/probudime e-ink
  eink_init();  
  // Posleme bitmapu do frame bufferu e-inku a vyvolame refresh/prekresleni               
  Serial.print("Kreslim bitmapu...");
  eink_bitmapa(); 
  // Prepneme e-ink do usporneho/vypnuteho rezimu 
  Serial.println("Vypinam e-ink...");
  eink_spanek();
  
  // Vypneme Wi-Fi
  Serial.println("Odpojuji Wi-Fi...");
  WiFi.mode(WIFI_OFF);
  
  // Prepneme se do rezimu hlubokeho spanku
  // Z nej se probudime za hodinu a cele kolecko se zopakuje
  // Displej se tedy prekresluje jednou za hodinu
  Serial.println("Prechazim do hlubokeho spanku...");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(prodleva_s * 1000000ULL); // Registrace probuzeni timerem
  esp_deep_sleep_start();
}


// Smycka loop je prazdna, nikdy se totiz nespusti
// Cip ESP32 se probuzeni z hlubokeho spanku resetuje
// a program se spusti od zacatku. Pocitadlo probuzeni prezije,
// protoze jsme jej atributem RTC_DATA_ATTR ulozili do specialni a omezene RAM,
// ktera je pod napetim i v hlubokem spanku a prezije reset
void loop() {
  ;
}
