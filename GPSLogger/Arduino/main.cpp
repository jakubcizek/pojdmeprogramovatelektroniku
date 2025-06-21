/*

GPS Tracker 0.1 Beta - kód je živý a stále ve vývoji!

Hardware:
 - LaskaKit ESPink-Shelf-2.13: https://www.laskakit.cz/en/laskakit-espink-shelf-213-esp32-e-paper/
 - Anebo větší LaskaKit ESPink-Shelf-2.9" https://www.laskakit.cz/en/laskakit-espink-shelf-2-9-esp32-e-paper/ (variantu určují makra USE_SHELF_213 a USE_SHELF_290)
 - GPS přijímač Quescan G10A-F30: https://www.aliexpress.com/item/1005005621100756.html
 - Anebo jiný, který komunikuje skrze UART a posílá data ve formátu textových zpráv NMEA 0183
 - Tlačítko
 - 100uF kondenzátor mezi 3.3V a GND pro vyrovnání špiček v okamžiku sepnutí napájení (jinak hrozí brownout, v jeden okamžik se totiž spouští hladová GPS s nabíjecí baterií a displej)

SDK:
 - Arduino
 - Kód přeložený v Platformiu pro Visual Studio Code (https://platformio.org/platformio-ide), přiložený konfigurační soubor projektu platformio.ini
 - Konfigurační soubor pro platformio má dva typy sestavení shelf_213 a shelf_290 pro různé typy prototypovacích desek. Výchozí je shelf_213. Stačí změnit v platformio.ini
 - Součástí platformio.ini jsou také definice všech použitých knihoven, které se stáhnou automaticky z Platformio Registry

 Odkazy na použité knihovny pro Arduino v případě použití Arduino IDE apod:

 TinyGPS++:   https://github.com/mikalhart/TinyGPSPlus
 ArduinoJson: https://arduinojson.org/
 GxEPD2:      https://github.com/ZinggJM/GxEPD2
 AdafruitGFX: https://github.com/adafruit/Adafruit-GFX-Library

 Konfigurace chování přijímače: /config.json na SD kartě. Konfigurace se načte při prvním studeném startu (zapnutí/vypnutí desky ESPInk, přeprogramování atp. Během spánku se udržuje v RTC RAM)

*/

#include <Arduino.h>
#include <SD.h>                                                                // Práce s SD kartou
#include <TinyGPSPlus.h>                                                       // Dekodér GPS NMEA zpráv
#include <ArduinoJson.h>                                                       // Práce s JSON; konfigurační soubor config.json na SD
#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>                                                         // Práce s e-ink displejem
#include <Fonts/FreeSansBold9pt7b.h>                                           // Bitmapové písmo z Adafruit GFX

#define GPIO_DC                            17                                  // GPIO piny pro kontrolu e-ink displeje 
#define GPIO_RST                           16                                  // GPIO piny pro kontrolu e-ink displeje 
#define GPIO_BUSY                          4                                   // GPIO piny pro kontrolu e-ink displeje 
#define GPIO_POWER                         2                                   // GPIO pin pro spínání napájení e-inku a GPS
#define GPIO_SD_SS                         15                                  // GPIO pin pro SPI SS SD karty
#define GPIO_VBAT                          34                                  // GPIO pin pro čtení napětí baterie
#define GPIO_BUTTON                        27                                  // GPIO pin pro tlačítko

                                                                               // Výchozí hodnoty, které se použijí, pokud chybí záznam v /config.json
#define SERIAL_NMEA_BAUDRATE               38400                               // Vychozí rychlost GPS UART. Zpravidla 9600, nebo 38400, nebo 115200
#define FILENAME                           "/zaznam.txt"                       // Výchozí název souboru pro ukládání dat
#define MINIMUM_CHANGE_TO_LOG_IN_METERS    200                                 // Výchozí minimální vzdálenost dvou po sobě jdoucích fixů, abychom uložili polohu
#define GPS_FIX_TIMEOUT_MS                 60000                               // Výchozí timeout v milisekundách pro čekání na fix
#define WAKE_PERIOD_MINUTES                10                                  // Výchozí perioda probouzení v minutách
#define REQUIRED_FIXES                     4                                   // Výchozí počet fixů musíme získat (týká se UART režimu a NMEA zpráv)


#define BUTTON_DEBOUNCE_MS                 50                                  // Ochrana před mechanickými fluktuacemi stisku tlačítka v milisekundách (debouncing)
#define UART_INACTIVITY_TIMEOUT_MS         60000                               // Timeout v milisekundách, po kterém při neaktivitě opustíme UART režim a přepneme čip do spánku
#define UART_MODE_BUTTON_DOWN_THRESHOLD_MS 3000                                // Doba v ms, po kteoru musíme držet tlačítko, abychom se přepli di UART režimu (logika zatím počítá jen s tím, že tlačítko stiskneme v době spánku a nikoliv v bdělém stavu)
#define DEVIDER_RATIO                      1.769                               // Koeficient pro dělič napětí přo čtení napětí baterie skrze ADC
#define UART_STORE_TIMEOUT_MS              10000                               // Timeout pro ukládání dat na SD kartu skrze UART. Pokud během ukládání dat neodrazí žádné bajty, ukončíme kopírování 

#ifdef USE_SHELF_213                                                           // Pokud používáme Laskakit ESPink-Shelf s 2,13" displejem
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT>                              
  display(GxEPD2_213_B74(SS, GPIO_DC, GPIO_RST, GPIO_BUSY)); 
#endif 

#ifdef USE_SHELF_290                                                           // Pokud používáme Laskakit ESPink-Shelf s 2,9" displejem GDEY029T71H
GxEPD2_BW<GxEPD2_290_GDEY029T71H, GxEPD2_290_GDEY029T71H::HEIGHT>
  display(GxEPD2_290_GDEY029T71H(SS, GPIO_DC, GPIO_RST, GPIO_BUSY));
#endif

struct Fix {                                                                   // Univerzální struktura s fixem. Obecná příprava i pro jiné způsoby získávání polohy než pomocí tinyGPS++ a NMEA (I2C, uBlox API aj.)
  double lat;                                                                  // Zeměpisná délka 
  double lng;                                                                  // Zeměpisná šířka
  double alt;                                                                  // Nadmořská výška (u levných GPS přijímačů realtivně nepřesná (až desítky metrů) = vztažená ke zjednodušenému modelu Země)
  double speed;                                                                // Rychlost pohybu v km/h
  double hdop;                                                                 // Horizontální přenost HDOP
  uint8_t sats;                                                                // Počet satelitů
  uint8_t hour;                                                                // Čas a datum v UTC (výpočet toho poledníkového je relativně složitý, protože politické hranice, ale některé přijímače to umějí)
  uint8_t minute; 
  uint8_t second; 
  uint8_t day; 
  uint8_t month; 
  uint16_t year;
  uint32_t timetofix;                                                          // Doba, než jsme získali fix (první platnou a relativně přesnou polohu) v milisekundách
};
Fix fix;

HardwareSerial gpsSerial(2);                                                   // Pro čtení dat z GPS použijeme 3. hardwarový UART
TinyGPSPlus gps;                                                               // Objekt GPS dekodéru

                                                                               // Proměnné v RTC RAM přežijí hluboký spánek čipu ESP32 a následné probuzení
RTC_DATA_ATTR bool        rtcRamOk                     = false;                // Kontrolní proměnná, jestli máme data v RTC RAM. V takovém případě je rovna true                                         
RTC_DATA_ATTR double      lastLat                      = 0.0;                  // Poslední známá zeměpisná šířka
RTC_DATA_ATTR double      lastLng                      = 0.0;                  // Poslední známá zeměpisná délka
RTC_DATA_ATTR bool        hasLastPosition              = false;                // Kontrola, jestli máme poslední známou polohu
RTC_DATA_ATTR double      totalDistanceMeters          = 0.0;                  // Celková vzdálenost expedice v metrech. Pouze orientační; přčesnost limituje nízká frekvence ukládání dat a výpočet na kouli
RTC_DATA_ATTR uint32_t    logCounter                   = 0;                    // Počítadlo uložených poloh na SD
                                                                               // Konfigurační část RTC RAM s hodnotami, které načteme z /config.json na SD, anebo použijeme výchozí hodnoty
RTC_DATA_ATTR char        filename[80];                                        // Název souboru, do kterého ukládáme data
RTC_DATA_ATTR uint32_t    minimum_change_to_log_meters = 0;                    // Minimální vzdálenost dvou po sobě jdoucích fixů, abychom uložili polohu
RTC_DATA_ATTR uint32_t    gps_fix_timeout_ms           = 0;                    // Timeout v milisekundách pro čekání na fix             
RTC_DATA_ATTR uint16_t    wake_period_minutes          = 0;                    // Perioda probouzení v minutách
RTC_DATA_ATTR uint8_t     required_fixes               = 0;                    // Počet fixů musíme získat (týká se UART režimu a NMEA zpráv)
RTC_DATA_ATTR uint32_t    serial_nmea_baudrate         = 0;                    // Rychlost GPS UART


esp_reset_reason_t reset_reason;                                               // Důvod resetu čipu ESP32. Ukládáme na SD pro ladění
esp_sleep_wakeup_cause_t wakeup_reason;                                        // Důvod probuzeni čipu ESP32. Ukládáme na SD pro ladění

struct CellData { String text; const GFXfont* font; };                         // Struktura pro snadné psaní do několika logických buněk na displeji
CellData cells[6];                                                             // Displej jsme rozdělili na šest buněk: Vlevo nahoře (čas), vpravo nahoře (batere, statistika), dva řádky uprostřed a opět dva spodní rohy (počítadlo, soubor)

                                                                               // Deklarace všech uživatelských funkcí. Mohli bychom zapouzdřit do třídy a pro přehlednost umístit do separátního souboru

                                                                               // GUI funkce pro práci s displejem:
void addToCell(uint8_t cellID, const GFXfont* font, const char* fmt, ...);     // Funkce pro přidání texu do buňky na displeji (zatím jen v bufferu). Podpora formátování jako u printf a volba fontu
void refreshDisplay();                                                         // Funkce pro naplnění buněk a překreslení displeje aktuální polohou. Používáme po úspěšném zjisštění pozice
void updateTopRightStats();                                                    // Funkce pro naplnění buňky se statistikou vpravo nahoře (počet staelitů, důvod resetu/probuzení, napětí a nabití baterie...)
void refresh();                                                                // Funkce pro ruční překreslení displeje. Použijí se aktuální data v bufferu (buňkách)

                                                                               // Funkce pro práci s SD:             
bool loadConfig();                                                             // Funkce pro načtení konfigurace v JSON ze souboru /config.json
void logToFile(File &f);                                                       // Funkce pro uložení aktuálaní polohy do souboru. Používáme textový soubor. Každý záznam představuje nový řádek a hodnoty oddělené středníkem (CSV)
bool loadLastFixAndStats();                                                    // Funkce pro načtení poslední známé polohy z SD. Používáme tehdy, když není poslendí poloha v RTC RAM (Vypnutí krabičky, přeprogramování atp.)

                                                                               // Ostatní funkce:
char getResetReason(esp_reset_reason_t reason);                                // Funkce pro převod numerického kódu důvodu resetu na zástupné písmeno, které zobrazujeme na displeji (B = brownout/máme problém, W = wake/probuzení atp.)
void goodNight();                                                              // Funkce pro nastavení probuzní procesoru časovačem, stiskem tlačítka a následné přepnutí procesoru do úsporného hlubokého spánku 
bool wasButtonPressed();                                                       // Funkce pro detekci stisku tlačítka 
void waitForSerialDataOrTimeoutOrButton();                                     // Funkce pro čekání na sériová data v UART režimu. čekání lze přerušit stiskem tlačítka, anebo vypršením timeoutu. Blokující funkce  
int vbatToPercent(float vbat);                                                 // Funkce pro převod napětí baterie na procenta. Experimentální, stále ladím (graf poklesu napětí baterie není lineární)
double distanceBetween(double lat0, double lng0, double lat1, double lng1);    // Výpočet ortodromy – nejkratší spojnice dvou míst na kouli (přesnsot v řádu procent, optimalziace pro oblasti severně od robníku a jižně od pólu)

void setup() {                                                                 // Hlavní funkce setup, začátek programu
  setCpuFrequencyMhz(40);                                                      // Snížíme frekvenci CPU na 40 MHz. Nepracujeme s Wi-Fi, takže to bohatě stačí. Během čekání na GPS ušetříme hromadu energie 
  delay(500);                                                                  // Elektrická stabilizace po probuzení. Nechcme žádné proudové výkyvy na samotném čipu, než se pustíme do práce. Pozůstatek první verze, kdy jen ESP32 na plný MHz výkon. Nechci do toho už ale raději hrabat; možné experimentování/zrušení
  reset_reason = esp_reset_reason();                                           // Zjištění důvodu resetu čipu ESP32
  wakeup_reason = esp_sleep_get_wakeup_cause();                                // Zjištění důvodu probuzenmí čipu ESP32
  Serial.begin(115200);                                                        // Nastartování systémové sériové linky rychlostí 115200 b/s. Jne pro ladění a UART režim při dlouhém stiksu tlačítka během spánku
  Serial.printf("Reset: %d\nWakeup: %d\n", reset_reason, wakeup_reason);       // Vypíšeme numerické kódy důvodu resetu a probuzení. Viz https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/misc_system_api.html 
  pinMode(GPIO_BUTTON, INPUT);                                                 // Nastavíme GPIO pin tlačítka na čtení 
  pinMode(GPIO_POWER, OUTPUT); digitalWrite(GPIO_POWER, HIGH);                 // Nastavíme GPIO pin napájení displeje a GPS na zápis a nastavíme vysoký stav = připojíme periferie k 3,3V napájení. V jeden okamžik se spíná GPS i displej, hrozí proudová špička a podpětí/brownout. Mezi 3,3V a GND jsem proto připájel 100uF kondenzátor
  display.init(115200, false, 2, false);                                        // Inicializujeme displej

  if (!SD.begin(GPIO_SD_SS)) {                                                 // Inicializujeme SD kartu. Při chybě vypíšeme na displej chybové hlášení a přepneme se do hlubokého spánku
    updateTopRightStats();
    addToCell(2, &FreeSansBold9pt7b, "Chyba SD!");
    addToCell(3, &FreeSansBold9pt7b, "Opakuji za %d minut", wake_period_minutes);
    refresh();
    display.powerOff();
    digitalWrite(GPIO_POWER, LOW);
    Serial.printf("Prepinam do deep-sleep na %d minut...\n", wake_period_minutes);
    Serial.flush();
    goodNight();
  }else{
    Serial.println("SD OK");
  }

  if(!rtcRamOk){                                                               // Pokud kontrolní proměnná v RTC RAM vrací (výchozí) false, znamená to, že v RTC RAM zatím nemáme konfigurační a statistická data. A proto je načteme z SD, nebo nastavíme výchozí hodnoty 
    if(loadConfig()){
      Serial.println("Nactena konfigurace z SD:");
      Serial.printf(" - Soubor: %s\n", filename);
      Serial.printf(" - GPS baudrate: %d b/s\n", serial_nmea_baudrate);
      Serial.printf(" - Pocet uspesnych fixu: %d\n", required_fixes);
      Serial.printf(" - GPS timeout: %d ms\n", gps_fix_timeout_ms);
      Serial.printf(" - Perioda probouzeni: %d minut\n", wake_period_minutes);
    }else{
      strncpy(filename, FILENAME, sizeof(filename) - 1); 
      filename[sizeof(filename) - 1] = '\0';
      minimum_change_to_log_meters = MINIMUM_CHANGE_TO_LOG_IN_METERS;
      gps_fix_timeout_ms = GPS_FIX_TIMEOUT_MS;
      wake_period_minutes = WAKE_PERIOD_MINUTES;
      required_fixes = REQUIRED_FIXES;
      serial_nmea_baudrate = SERIAL_NMEA_BAUDRATE;
      Serial.println("Nepodarilo se nacist konfiguraci z SD, volim vychozi hodnoty:");
      Serial.printf(" - Soubor: %s\n", filename);
      Serial.printf(" - GPS baudrate: %d b/s\n", serial_nmea_baudrate);
      Serial.printf(" - Pocet uspesnych fixu: %d\n", required_fixes);
      Serial.printf(" - GPS timeout: %d ms\n", gps_fix_timeout_ms);
      Serial.printf(" - Perioda probouzeni: %d minut\n", wake_period_minutes);
    }
    if(loadLastFixAndStats()){
      Serial.println("Nactena statistika z SD:");
      Serial.printf(" - Pocitadlo zaznamu: %d\n", logCounter);
      Serial.printf(" - Celkova vzdalenost: %.2f m\n", totalDistanceMeters);
      Serial.printf(" - Posledni z.s: %f\n", lastLat);
      Serial.printf(" - Posledni z.d.: %f\n",lastLng);
    }
  }

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {                                // Pokud je důvodem probuzení přerušení typu EXT0 (změna stavu na GPIO, tedy stisk našeho tlačítka)
    uint32_t t0 = millis();
    while (digitalRead(GPIO_BUTTON) == HIGH) {                                 // Pokud tlačítko stále držíve (čteme na pinu vysoký stav), měříme čas    
      if (millis() - t0 >= UART_MODE_BUTTON_DOWN_THRESHOLD_MS) {               // Pokud tlačítko držíme dostatečně dlouho, překreslíme displej a přepneme se do UART režimu = spustíme blokovací funkci pro poslech sériové linky 
        updateTopRightStats();
        addToCell(2, &FreeSansBold9pt7b, "Rezim USB");
        addToCell(3, &FreeSansBold9pt7b, "115200 b/s UART");
        refresh();
        display.powerOff();
        digitalWrite(GPIO_POWER, LOW);
        waitForSerialDataOrTimeoutOrButton();
      }
      delay(10);
    }
  }
 
  gpsSerial.begin(serial_nmea_baudrate, SERIAL_8N1, SDA, SCL);                 // Nastartujeme sériovou linku do GPS přijímače, který jsme připojili skrze konektor uŠup. Ten primárně slouží pro sběrnici I2C, a proto dáme procesoru vědět, že má pro UART používat piny SDA a SCL 

  if(!rtcRamOk || wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){                     // Pokud se jedná o nový start (RTC RAM je prázdná), nebo o start způsobený tlačítkem, překreslíme displej hláškou "Hledam GPS". Při dalších probuzeních to už dělat nebudeme, protože je to nadbytečná spotřeba energie
    updateTopRightStats();
    addToCell(2, &FreeSansBold9pt7b, "Hledam GPS...");
    refresh();
  }


  Serial.printf("Cekam na GPS fix nejvyse %d ms... ", gps_fix_timeout_ms);
  uint32_t t0 = millis();
  bool timeout = true;
  bool firstFix = true;
  uint8_t fixes = 0;                                                           // Čekáme ve smyčce na GPS data. Ze smyčky vyskočíme po přijetí stanovewného počtu poloh, nebo po vypršení timeoutu
  while (millis() - t0 < gps_fix_timeout_ms) {
    while (gpsSerial.available()){
      char c = gpsSerial.read();
      gps.encode(c);
    }
    if (gps.location.isValid() && 
        gps.altitude.isValid() && 
        gps.time.isValid() && 
        gps.date.isValid() && 
        gps.date.year() >= 2025) {
      if(firstFix){
        firstFix = false;
        fix.timetofix = millis() - t0;
        Serial.printf("%d ms\n", fix.timetofix);
      }
      if (gps.location.isUpdated() && 
          gps.altitude.isUpdated() && 
          gps.time.isUpdated()) {
        fixes++;
        Serial.printf("Fix #%d: %.6f, %.6f, %.2f m\n", 
            fixes, 
            gps.location.lat(), 
            gps.location.lng(), 
            gps.altitude.meters()
        );
                                                                               // Pokud jsme získali stanovený počet validních poloh, tu poslední uložíme do struktury fix
                                                                               // Předpokládáme, že čekáním alespoň na několik po sobě jdoucích poloh se výpočet na straně GPS přijímače
                                                                               // postupně zpřesňuje. Mohli bychom pracovat i s HDOP, počtem družic atp., ale pak hrozí, že někde v údolí se slabým příjmem
                                                                               // bychom nikdy nesplnili podmínku. Bereme to tak, že chceme to nejlepší dostupné, ať už to má jakoukoliv kvalitu
        if (fixes >= required_fixes) {
          fix.lat = gps.location.lat();
          fix.lng = gps.location.lng();
          fix.alt = gps.altitude.meters();
          fix.speed = gps.speed.kmph();
          fix.hdop = gps.hdop.value();
          fix.sats = gps.satellites.value();
          fix.day = gps.date.day();
          fix.month = gps.date.month();
          fix.year = gps.date.year();
          fix.hour = gps.time.hour();
          fix.minute = gps.time.minute();
          fix.second = gps.time.second();
          timeout = false;
          break;
        }
      }
    }
  }

  if(timeout){                                                                 // V případě timoutu vypíšeme chybové hlášení a přepneme se do hlubokého spánku
    Serial.println("Timeout!");
    Serial.println("Nemam fix! Usnu a zkusim to znovu v dalsim kole");
    updateTopRightStats();
    addToCell(2, &FreeSansBold9pt7b, "Nemohu najit GPS!");
    addToCell(3, &FreeSansBold9pt7b, "Opakuji za %d minut", wake_period_minutes);
    refresh();
    display.powerOff();
    digitalWrite(GPIO_POWER, LOW);
    Serial.printf("Prepinam do deep-sleep na %d minut...\n", wake_period_minutes);
    Serial.flush();
    goodNight();
  }

  
  File logFile = SD.open(filename, FILE_APPEND);                               // Pokusíme se otevřít soubor pro záznam hodnot. Pokud se to nepodaří, vypíšeme chybové hlášení a přepneme se do hlubokého spánku
  if (!logFile) {
    Serial.println("Nelze otevrit soubor!");
    updateTopRightStats();
    addToCell(2, &FreeSansBold9pt7b, "Nelze otevrit soubor!");
    addToCell(3, &FreeSansBold9pt7b, "Opakuji za %d minut",  wake_period_minutes);
    refresh();
    display.powerOff();
    digitalWrite(GPIO_POWER, LOW);
    Serial.printf("Prepinam do deep-sleep na %d minut...\n",  wake_period_minutes);
    Serial.flush();
    goodNight();
  }

  logToFile(logFile);                                                          // Uložíme hodnoty do textového souboru a soubor zavřeme                                                  
  logFile.close();

  refreshDisplay();                                                            // Překreslíme displej údaji o aktuální poloze
  display.powerOff();                                                          // Vypneme displej
  digitalWrite(GPIO_POWER, LOW);                                               // Odpojíme napájení periferií (GPS přijímač a displej)
  Serial.printf("Prepinam do deep-sleep na %d minut...\n",  wake_period_minutes);
  Serial.flush();
  goodNight();                                                                 // Přepneme procesor do hlubokého spánku
}

void loop() {                                                                  // Povinná smyčka loop v Arduinu se fakticky nikdy nespustí
  ;
}

                                                                              // Kód uživatelských funkcí:
                                                                              // Mohli bychom je zapouzdřit do třídy a izolovat do knihovny/sekundárního *.cpp/*.h souboru pro lepší čitlenost hlavního programu main.cpp 

bool loadConfig() {                                                           // Otevřeme a přečteme soubor /config.json, pomocí ArduinoJson dekódujeme JSON a nastavíme konfigurační globánlí proměnné
  File configFile = SD.open("/config.json", FILE_READ);
  if (!configFile) {
    Serial.println("Nepodarilo se otevrite /config.json");
    return false;
  }
  size_t size = configFile.size();
  if (size > 2048) {
    Serial.println("Soubor je prilis velky");
    configFile.close();
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size + 1]);
  configFile.readBytes(buf.get(), size);
  buf[size] = '\0';
  configFile.close();
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.print("Chyba při parsování JSON: ");
    Serial.println(error.f_str());
    return false;
  }
  const char* fn = doc["filename"];
  if (fn) {
    strncpy(filename, fn, sizeof(filename));
    filename[sizeof(filename) - 1] = '\0';  // vždy zakonči řetězec
  } else {
    strncpy(filename, FILENAME, sizeof(filename) - 1); 
    filename[sizeof(filename) - 1] = '\0';
  }
  minimum_change_to_log_meters = doc["minimum_change_to_log_meters"] | MINIMUM_CHANGE_TO_LOG_IN_METERS;
  gps_fix_timeout_ms = doc["gps_fix_timeout_ms"] | GPS_FIX_TIMEOUT_MS;
  wake_period_minutes = doc["wake_period_minutes"] | WAKE_PERIOD_MINUTES;
  required_fixes = doc["required_fixes"] | REQUIRED_FIXES;
  serial_nmea_baudrate = doc["serial_nema_baudrate"] | SERIAL_NMEA_BAUDRATE;
  return true;
}

bool loadLastFixAndStats() {                                                   // Přečteme poslední plný řádek TXT/CSV souboru, do kterého ukládáme informace o poloze a vytáhneme z něj poslední zaznamenané souřadnice, počítadlo a celkovo uvzdálenost  
  File f = SD.open(filename, FILE_READ);
  if (!f) {
    Serial.printf("Soubor %s se nepodařilo otevřít pro získání poslední známé polohy\n", filename);
    return false;
  }
  const size_t maxLine = 256;
  char line[maxLine] = {0};
  size_t size = f.size();
  if (size < 2) {
    f.close();
    return false;
  }
  int newlineCount = 0;
  int64_t pos = size - 1;
  while (pos >= 0 && newlineCount < 2) {
    f.seek(pos);
    char c = f.read();
    if (c == '\n') newlineCount++;
    pos--;
  }
  f.seek(pos + 2);
  int len = f.readBytesUntil('\n', line, maxLine - 1);
  line[len] = '\0';
  f.close();
  if (len == 0) return false;
  char *token;
  int field = 0;
  uint32_t tmpLogCount = 0;
  double tmpTotalDist = 0.0f;
  double tmpLastLoggedLat = 0.0f;
  double tmpLastLoggedLng = 0.0f;
  token = strtok(line, ";");
  while (token != nullptr) {
    if (field == 2) {
      tmpLogCount = atoi(token);
    } else if (field == 3) {
      tmpLastLoggedLat = atof(token);
    } else if (field == 4) {
      tmpLastLoggedLng = atof(token);
    } else if (field == 7) {
      tmpTotalDist = atof(token) * 1000.0f;
    }
    token = strtok(nullptr, ";");
    field++;
  }
  if (field < 7) return false;
  logCounter = tmpLogCount;
  totalDistanceMeters = tmpTotalDist;
  hasLastPosition = true;
  lastLat = tmpLastLoggedLat;
  lastLng = tmpLastLoggedLng;
  return true;
}

void updateTopRightStats(){                                                    // Do buňky 1 napíšeme počet satelitů při posledním fixu, důvod resetu/probuzení a napětí (V) a nabití (%) baterie
  float vbat = analogReadMilliVolts(GPIO_VBAT) * DEVIDER_RATIO / 1000;
  addToCell(
    1, 
    &FreeSansBold9pt7b, 
    "%dS %c %.2fV (%d%%)", fix.sats,
    getResetReason(reset_reason), 
    vbat, vbatToPercent(vbat)
  ); 
}
                                                                               // Výpočet nabití baterie z rozsahu napětí 3-4,2V (minimální a maximální napětí) není lineární, protože grarf vybíjení připomíná sigmoidu (na začátku rychlý pokles, pak rovina a následně opět rychlý pokles)
                                                                               // Toto je zatím opravdu betaverze, která vyžaduje další ladění podle toho, jak se bude akumulátor opravdu vybíjet v konkrétních podmínkách
                                                                               // Je dost možné, že prostý lineární přepočet nakonec bude spolehlivější. Anebo třeba jen znázornění po 25% krocích: 100% 75% 50% 25% 0%
                                                                               // Mohli bychom také dodělat logiku, že při poklesu napětí/nabití pod určitou hranici překreslíme displej varovným hlášením a usneme. To zatím není implementováno 
int vbatToPercent(float vbat){                                                
  int percent;
  if (vbat >= 4.2) percent = 100;
  else if (vbat <= 3.0) percent = 0;
  else {
    float v = (vbat - 3.0) / (4.2 - 3.0);
    percent = (int)(100.0 * v * v);
  }
  return percent;
}
                                                                               // Nastavíme časovač probuzení, probuzení externím signálem (tlačítko) a nastavíme kontrolní proměnnou v RTC RAM na true
                                                                               // Pokud bude true i při dalším probuzení, data v RTC RAM (pravděpodobně) přežila bez úhony
                                                                               // Poté čip přepneme do hlubokého spánku                                           
  
void goodNight(){  
  rtcRamOk = true; 
  esp_sleep_enable_timer_wakeup((uint64_t) wake_period_minutes * 60ULL * 1000000UL);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)GPIO_BUTTON, 1); 
  esp_deep_sleep_start();
}

bool wasButtonPressed() {                                                      // Detekce stisku tlačítka včetně ošetření na roztřesený signál způsobený mechanickým stiskem (debouncing), abychom omylem nepřečetli vícero stiků 
  static bool prevState = false;
  bool currentState = digitalRead(GPIO_BUTTON);
  if (currentState && !prevState) {
    delay(BUTTON_DEBOUNCE_MS);
    if (digitalRead(GPIO_BUTTON)) {
      prevState = true;
      return true;
    }
  } else if (!currentState) {
    prevState = false;
  }
  return false;
}
                                                                               // Smyčka UART režimu:
                                                                               // V UART režimu posloucháme na primární/USB sériové lince (115200 b/s), pokud nedoraxí tyto povely:
                                                                               //   ls/n         – vrátíme seznam soubprů na SD kartě
                                                                               //   cat SOUBOR\n – vypíšeme obsah souboru (předpokládáme, že se jedná o text)
                                                                               //   rm SOUBOR\n  - smažeme soubor
                                                                               // Odpověď vždy ve formátu: response;cmd:PRIKAZ;DATA
                                                                               // Příklad pro ls\n:
                                                                               //   response;cmd:ls;data:soubor1,soubor2,soubor3...
                                                                               // Příklad pro cat soubor1.txt\n:
                                                                               //   response;cmd:cat;filename:soubor1.txt;size:5;data:
                                                                               //   ahoj
                                                                               //
                                                                               // Čekání na data lze eukončit krátkým stiskem tlačítka, anebo při neaktivitě (žádná sériová data po stanovený čas) 
                                                                               // Na začátku ještě počkáme, dokud uživatel neuvolní tlačítko, pokud ho stále drží, jinak bychom trvající stisk rovnou interpretovali jako ukončení režimu UART
void waitForSerialDataOrTimeoutOrButton() {
  while (digitalRead(GPIO_BUTTON) == HIGH) {
    delay(10);
  }
  uint32_t lastActivity = millis();
  while (millis() - lastActivity < UART_INACTIVITY_TIMEOUT_MS) {
    if (Serial.available()) {
      while (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if(cmd == "ls"){
          String response = "response;cmd:ls;data:";
          File dir = SD.open("/");
          while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;
            response += String(entry.name()) + ",";
            entry.close(); 
          }
          dir.close();
          Serial.println(response);
        }
        else if (cmd.startsWith("cat ")) {
          String filename = cmd.substring(4);
          filename.trim();

          File f = SD.open("/" + filename);
          if (f && !f.isDirectory()) {
            String response = "response;cmd:cat;filename:" + filename + ";size:" + String(f.size()) + ";data:\n";
            Serial.print(response);

            while (f.available()) {
              Serial.write(f.read());
            }
            Serial.println();
            f.close();
          } else {
            Serial.printf("response;cmd:cat;filename:%s;size:0;data:\n", filename.c_str());
          }
        }
        else if (cmd.startsWith("rm ")) {
          String filename = cmd.substring(3);
          filename.trim();

          if (SD.exists("/" + filename)) {
            if (SD.remove("/" + filename)) {
              Serial.printf("response;cmd:rm;filename:%s;status:ok\n", filename.c_str());
            } else {
              Serial.printf("response;cmd:rm;filename:%s;status:error\n", filename.c_str());
            }
          } else {
            Serial.printf("response;cmd:rm;filename:%s;status:not_found\n", filename.c_str());
          }
        }
        else if (cmd.startsWith("store ")) {
          int8_t firstSpace = cmd.indexOf(' ');
          int8_t secondSpace = cmd.indexOf(' ', firstSpace + 1);

          if (firstSpace == -1 || secondSpace == -1) {
            Serial.println("response;cmd:store;status:error\n");
            return;
          }

          String filename = cmd.substring(firstSpace + 1, secondSpace);
          String lengthStr = cmd.substring(secondSpace + 1);
          filename.trim();
          lengthStr.trim();

          int expectedSize = lengthStr.toInt();
          if (expectedSize <= 0) {
            Serial.printf("response;cmd:store;filename:%s;status:error\n", filename.c_str());
            return;
          }

          File f = SD.open("/" + filename, FILE_WRITE);
          if (!f) {
            Serial.printf("response;cmd:store;filename:%s;status:error\n", filename.c_str());
            return;
          }

          int bytesReceived = 0;
          uint32_t start = millis();
          while (bytesReceived < expectedSize) {
            if (Serial.available()) {
              int b = Serial.read();
              f.write((uint8_t)b);
              bytesReceived++;
            } else {
              if (millis() - start > UART_STORE_TIMEOUT_MS) {
                f.close();
                SD.remove("/" + filename);
                Serial.printf("response;cmd:store;filename:%s;status:timeout\n", filename.c_str());
                return;
              }
            }
          }

          f.close();
          Serial.printf("response;cmd:store;filename:%s;status:ok\n", filename.c_str());
        }

      }
      lastActivity = millis();
    }
    if (wasButtonPressed()) {
      while (digitalRead(GPIO_BUTTON) == HIGH) {
        delay(10);
      }
      pinMode(GPIO_POWER, OUTPUT); digitalWrite(GPIO_POWER, HIGH);
      display.init(115200,true,2,false);
      updateTopRightStats();
      addToCell(2,&FreeSansBold9pt7b,"Ukoncuji USB");
      addToCell(3,&FreeSansBold9pt7b,"Mereni za %d minut",  wake_period_minutes);
      refresh();
      display.powerOff();
      digitalWrite(GPIO_POWER, LOW);
      goodNight();
    }
    delay(1);
  }
  pinMode(GPIO_POWER, OUTPUT); digitalWrite(GPIO_POWER, HIGH);
  display.init(115200,true,2,false);
  updateTopRightStats();
  addToCell(2,&FreeSansBold9pt7b,"Ukoncuji USB");
  addToCell(3,&FreeSansBold9pt7b,"Mereni za %d minut",  wake_period_minutes);
  refresh();
  display.powerOff();
  digitalWrite(GPIO_POWER, LOW);
  goodNight();
}

char getResetReason(esp_reset_reason_t reason) {                               // Převod důvodu resetu/probuzení z numerického kódu na snáze zapamatovatelný znak, který se zobrazuje na displeji v pravé horní části
  switch (reason) {                                    
    case ESP_RST_POWERON:     return 'P';                                      // Power-on reset
    case ESP_RST_SW:          return 'S';                                      // Software reset (esp_restart())
    case ESP_RST_PANIC:       return 'C';                                      // Kernel panic / crash
    case ESP_RST_INT_WDT:     return 'G';                                      // Watchdog z přerušení
    case ESP_RST_TASK_WDT:    return 'G';                                      // Watchdog z úkolu (task)
    case ESP_RST_WDT:         return 'G';                                      // Jiný watchdog
    case ESP_RST_DEEPSLEEP:                                                    // Wake from deep sleep
      if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
        return 'T';                                                            // Probuzení tlačítkem
      }else{
        return 'W';                                                            // Probuzení časovačem
      }
    case ESP_RST_BROWNOUT:    return 'B';                                      // Brownout detekce
    default:                  return '?';                                      // Neznámá nebo nepodporovaná hodnota
  }
}

void addToCell(uint8_t cellID, const GFXfont* font, const char* fmt, ...) {    // Napiš text do buňky 0-5 (zleva doprava, shora dolů)
  if (cellID >= 6 || !fmt || !font) return;
  char buf[128];
  va_list args; va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  cells[cellID].text = String(buf);
  cells[cellID].font = font;
}

void refreshDisplay(){                                                         // Naplň všechny buňky aktuálními daty o poloze
  
  addToCell(                                                                   // Buňka 0: Čas fixu          
    0, 
    &FreeSansBold9pt7b, 
    "%02d:%02d:%02d", fix.hour, fix.minute, fix.second
  );
  
   updateTopRightStats();                                                      // Buňka 1: Počet satelitů, iniciátor startu, napětí a nabití baterie  
  
  addToCell(                                                                   // Buňka 2: Zeměpisná šířka a délka 
    2, 
    &FreeSansBold9pt7b, 
    "%.7f %.7f", fix.lat, fix.lng
  );  
  
  addToCell(                                                                   // Buňka 3: Nadmořská výška a celková vzdálenost
    3, 
    &FreeSansBold9pt7b, 
    "%.0f m n.m. %.2f km", 
    fix.alt, (totalDistanceMeters / 1000.0f)
  );  
  
  addToCell(                                                                   // Buňka 4:Počet uložených záznamů na SD
    4, 
    &FreeSansBold9pt7b, 
    "%d", logCounter
  );
  
  addToCell(                                                                   // Buňka 5: Název souboru na SD
    5, 
    &FreeSansBold9pt7b, 
    "%s", FILENAME
  );
  
  refresh();
}

void refresh() {                                                               // Vykreslíme buňky na displej
  display.setTextColor(GxEPD_BLACK);
  display.setRotation(3);
  int16_t w = display.width();
  int16_t h = display.height();
  const int margin = 5;
  struct CellPos { int x, y; };
  CellPos positions[6] = {
    {margin, margin},
    {w - margin, margin},
    {w / 2, h / 2 - 10},
    {w / 2, h / 2 + 10},
    {margin, h - margin},
    {w - margin, h - margin}
  };
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    for (int i = 0; i < 6; ++i) {
      if (cells[i].text.length() == 0 || cells[i].font == nullptr) continue;
      display.setFont(cells[i].font);
      int16_t x1, y1;
      uint16_t tw, th;
      display.getTextBounds(cells[i].text, 0, 0, &x1, &y1, &tw, &th);
      int x = positions[i].x;
      int y = positions[i].y;
      if (i == 2 || i == 3) x -= tw / 2;
      else if (i == 1 || i == 5) x -= tw;
      if (i == 0 || i == 1) y -= y1;
      else if (i == 4 || i == 5) y -= (y1 + th);
      else y -= y1 / 2;
      display.setCursor(x, y);
      display.print(cells[i].text);
    }
  } while (display.nextPage());
}

double distanceBetween(double lat0, double lng0, double lat1, double lng1) {   // Výpočet vzdálenosti mezi dvěma body. Nepřesné! Pouze orientační a s chybou
  double delta = radians(lng0-lng1);
  double sdlong = sin(delta);
  double cdlong = cos(delta);
  lat0 = radians(lat0);
  lat1 = radians(lat1);
  double slat0 = sin(lat0);
  double clat0 = cos(lat0);
  double slat1 = sin(lat1);
  double clat1 = cos(lat1);
  delta = (clat0 * slat1) - (slat0 * clat1 * cdlong);
  delta = sq(delta);
  delta += sq(clat1 * sdlong);
  delta = sqrt(delta);
  double denom = (slat0 * slat1) + (clat0 * clat1 * cdlong);
  delta = atan2(delta, denom);
  return delta * 6372795;                                                      // Střední poloměr Země: Přesnější ve středních změmepisných šířkách, méně na rovníku a v polárních oblastech
}

void logToFile(File &f) {                                                      // Uložíme fixu do CSV souboru na SD
  double d = 0;
  if (hasLastPosition) {
    d = distanceBetween(fix.lat, fix.lng, lastLat, lastLng);
    if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
      Serial.println("Ignoruji kontrolu vzdalenosti mezi dvema body");
    }else{
      if (d < minimum_change_to_log_meters) {
        Serial.printf("Rozdil poloh pouze %.1f m, neukladam\n", d);
        return;
      }else{
        Serial.printf("Rozdil poloh %.1f m, ukladam\n", d);
      }
    }
    totalDistanceMeters += d;
  }
  logCounter++;
  float vbat = analogReadMilliVolts(GPIO_VBAT) * DEVIDER_RATIO / 1000.0f;
  lastLat = fix.lat;
  lastLng = fix.lng;
  hasLastPosition=true;
  char line[512];
  snprintf(line,sizeof(line),
    "%02d.%02d. %04d;%02d:%02d:%02d;%d;%.6f;%.6f;%.1f;%.1f;%.2f;%.1f;%d;%d;%.1f;%.3f;%d;%d;%d\n",
    fix.day, fix.month, fix.year,                                              // DD.MM. YYYY
    fix.hour, fix.minute, fix.second,                                          // HH:MM:SS
    logCounter,                                                                // Počítalo uložených záznamů
    fix.lat,                                                                   // Zeměpisná šířka
    fix.lng,                                                                   // Zeměpisná délka
    fix.alt,                                                                   // Nadmořská výška (m)
    fix.speed,                                                                 // Rychlost (km/h)
    totalDistanceMeters/1000.0,                                                // Celková vzdálenost (km)
    d,                                                                         // Vzdálenost mezi po sobě jdoucími fixy                         
    fix.timetofix,                                                             // Doma k získání prvního fixu (ms)
    fix.sats,                                                                  // Počet staelitů
    fix.hdop,                                                                  // HDOP / Horizontální přesnost
    vbat,                                                                      // Napětí baterie (V)
    vbatToPercent(vbat),                                                       // Nabití baterie (%)
    reset_reason,                                                              // Důvod resetu
    wakeup_reason                                                              // Důvod probuzeni
  );
  f.print(line);
  f.flush(); 
}
