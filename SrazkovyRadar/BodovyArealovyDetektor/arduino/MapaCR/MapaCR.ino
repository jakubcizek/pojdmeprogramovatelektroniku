// Knihovny pro Wi-Fi a HTTP
// Soucasti podpory pro citpy ESP32
// https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

#include <Adafruit_NeoPixel.h> // Je treba doinstalovat ze spravce knihoven Arduino
#include <ArduinoJson.h>  // Je treba doinstalovat ze spravce knihoven Arduino

// NAZEV A HESLO K WIFI
// 802.11bgn a pouze 2.4 GHz
const char *ssid = "nazev wifi";
const char *heslo = "heslo wifi";


// URL SERVERU, SE KTERYM BUDEME KOMUNIKOVAT
// POZOR, JEN PRIKLAD KOMUNIAKCE V LOKALNI SITI LAN,
// POKUD SERVER POBEZI NA IP ADRESE 192.168.1.10
String api1 = "http://192.168.1.10/?mereni=bodove";
String api2 = "http://192.168.1.1/?mereni=arealove";

/*

MUZETE POUZIT I MOJE VEREJNE API DOSTUPNE ODKUDKOLIV Z INTERNETU
V PRIPADE TOHOTO API NICMENE NEGARANTUJI VECNY PROVOZ

String api1 = "http://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni.json";
String api2 = "http://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni_v2.json";

*/

// Vychozi volba zdroje JSON
// Budeme pouzivat API 2, tedy arealove mereni
int api = 2;

// Objekt pro ovladani adresovatelnych RGB LED
// Je jich 72 a jsou v serii pripojene na GPIO pin 25
Adafruit_NeoPixel pixely(72, 25, NEO_GRB + NEO_KHZ800);
// HTTP server bezici na standardnim TCP portu 80
WebServer server(80);
// Pamet pro JSON s povely
// Alokujeme pro nej 10 000 B, coz je hodne,
// ale melo by to stacit i pro jSON,
// ktery bude obsahovat instrukce pro vsech 72 RGB LED
StaticJsonDocument<10000> doc;

uint32_t t = 0;             // Refreshovaci timestamp
uint32_t delay10 = 600000;  // Prodleva mezi aktualizaci dat, 10 minut
uint8_t jas = 5;            // Vychozi jas

uint32_t t2 = 0;           // Refreshovaci timestamp pro volitelne blikani stare a nove barvy konkretniho mesta
uint32_t delay1 = 1000;    // Prodleva mezi blikanim 1 s
bool tick = false;         // Stav bliknuti

bool blikani_povoleno = false; // Povoleni blikani

// Rezim zadniho ambientniho podsviceni, pokud na GPIO 2 pripojime modry zdroj svetla 
// a na GPIO 13 modry zdroj svetla. Nic z toho neni soucasti baleni, nalistujte si ale 
// vydani casopisu Computer (zari 2023), ve kterem jsme si pohrali s podsvicenim ve tvaru
// ohebne snurky z AliExpressu za par dolaru
uint8_t rezim_podsviceni = 0; 

// Struktura RGB LED pro blikani starou a novou barvou
// Slouzi pro znazorneni zmeny v poslednich 10-20 minutach
typedef struct {
  bool blika;
  uint8_t r0;
  uint8_t g0;
  uint8_t b0;
  uint8_t r1;
  uint8_t g1;
  uint8_t b1;
} bod;

// Historie RGB LED pro predchozi a novy stav/barvu
bod historie[72];

// Dekoder JSONu a rozsvecovac svetylek
int jsonDecoder(String s, bool log) {
  DeserializationError e = deserializeJson(doc, s);
  if (e) {
    if (e == DeserializationError::InvalidInput) {
      return -1;
    } else if (e == DeserializationError::NoMemory) {
      return -2;
    } else {
      return -3;
    }
  } else {
    pixely.clear();  // Smazeme pixely a pripravime se na novy frame
    JsonArray mesta = doc["seznam"].as<JsonArray>();
    bool dest = false;
    uint16_t celkova_modra = 0;
    uint16_t celkova_zelena = 0;
    for (JsonObject mesto : mesta) {
      int id = mesto["id"];
      int r = mesto["r"];
      int g = mesto["g"];
      int b = mesto["b"];
      historie[id].blika = false;
      pixely.setPixelColor(id, pixely.Color(r, g, b));

      celkova_modra += b;
      celkova_zelena += g;

      if (r + g + b > 0) dest = true;

      // Ulozime si predchozi a akutali stav RGB
      // pro ucely blikani
      historie[id].r0 = historie[id].r1;
      historie[id].g0 = historie[id].g1;
      historie[id].b0 = historie[id].b1;
      historie[id].r1 = r;
      historie[id].g1 = g;
      historie[id].b1 = b;

      // Pokud se nove barvy lisi od tech starych,
      // LED bude blikat starou a novou barvou
      if (historie[id].r0 != r || historie[id].g0 != g || historie[id].b0 != b) {
        historie[id].blika = true;
        if (log) Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d\r\n a aktivuji blikani\n", id, r, g, b);
      } else {
        historie[id].blika = false;
        if (log) Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d\r\n a deaktivuji blikani\n", id, r, g, b);
      }
    }
    pixely.show();
    // Pokud jsme k pinum 2 a 13 pripojili zeleny a modry zdroj svetla,
    // podle prevladajicicho zeleneho, nebo naopak modreho odstinu deste
    // se rozsviti adekvatni zdroj svetla
    if (dest && (rezim_podsviceni == 0)) {
      if (celkova_zelena >= celkova_modra) {
        digitalWrite(13, LOW);
        digitalWrite(2, HIGH);
      } else{
        digitalWrite(2, LOW);
        digitalWrite(13, HIGH);
      }    
    }else{
      digitalWrite(2, LOW);
      digitalWrite(13, LOW);
    }
    return 0;
  }
}

// Stazeni radarovych dat z webu
void stahniData() {
  HTTPClient http;
  if(api == 1) http.begin(api1);
  else if(api == 2) http.begin(api2);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int err = jsonDecoder(http.getString(), true);
    switch (err) {
      case 0:
        Serial.println("Hotovo!");
        break;
      case -1:
        Serial.println("Spatny format JSONu");
        break;
      case -2:
        Serial.println("Malo pameti, navys velikost StaticJsonDocument");
        break;
      case -3:
        Serial.println("Chyba pri parsovani JSONu");
        break;
    }
  }
  http.end();
}

// ESP32 si na sve IP adrese spsuti jendoduchy HTTP server pro jeho konfiguraci
// a pripadne i rizeneho nahrani JSONu s daty mest 
// Tuto funkci HTTP server zavola v pripade HTTP GET/POST pzoadavku na korenovou cestu /
void httpDotaz(void) {
  // Pokud HTTP data obsahuji parametr mesta
  // predame jeho obsah JSON dekoderu
  if (server.hasArg("mesta")) {
    int err = jsonDecoder(server.arg("mesta"), true);
    switch (err) {
      case 0:
        server.send(200, "text/plain", "OK");
        break;
      case -1:
        server.send(200, "text/plain", "CHYBA\nSpatny format JSON");
        break;
      case -2:
        server.send(200, "text/plain", "CHYBA\nMalo pameti RAM pro JSON. Navys ji!");
        break;
      case -3:
        server.send(200, "text/plain", "CHYBA\nNepodarilo se mi dekodovat jSON");
        break;
    }
  }
  // Pokud jsme do mapy poslali jen HTTP GET/POST parametr smazat, mapa zhasne
  else if (server.hasArg("smazat")) {
    server.send(200, "text/plain", "OK");
    pixely.clear();
    pixely.show();
  // Pokud jsme do mapy poslali jen HTTP GET/POST parametr jas, upravime jas
  // Priklad: http://ipadresa/?jas=3
  } else if (server.hasArg("jas")) {
    server.send(200, "text/plain", "OK");
    jas = server.arg("jas").toInt();
    pixely.setBrightness(jas);
    pixely.show();
  // Pokud jsme do mapy poslali jen HTTP GET/POST parametr blikani, aktivujeme blikani mezi historicko u a novou hodnotou
  } else if (server.hasArg("blikani")) {
    server.send(200, "text/plain", "OK");
    blikani_povoleno = (bool)server.arg("blikani").toInt();
  // Pokud jsme do mapy poslali jen HTTP GET/POST parametr api, vynutime si stazeni novych dat z externiho serveru
  // Priklad: http://ipadresa/?api=1
  } else if (server.hasArg("api")) {
    server.send(200, "text/plain", "OK");
    api = (int)server.arg("api").toInt();
    stahniData();
  // Pokud jsme do mapy poslali jen HTTP GET/POST parametr podsviceni, zmenime barvu podsviceni
  // Musime ale k mape pripajet vlastni zadni osvetleni, ktere neni soucasti baleni!
  // Zelene osvetleni ovlada pin 13, modre podsviceni pin 2
  // Priklad: http://ipadresa/?podsviceni=1
  } else if (server.hasArg("podsviceni")) {
    server.send(200, "text/plain", "OK");
    rezim_podsviceni = server.arg("podsviceni").toInt();
    if (rezim_podsviceni == 1) {
      digitalWrite(13, LOW); //  ZELENE
      digitalWrite(2, HIGH);
    } else if (rezim_podsviceni == 2) { // MODRE
      digitalWrite(2, LOW);
      digitalWrite(13, HIGH);
    } else if (rezim_podsviceni == 3) { // ZADNE
      digitalWrite(2, LOW);
      digitalWrite(13, LOW);
    }
  }
  // Ve vsech ostatnich pripadech odpovime chybovym hlasenim
  else {
    server.send(200, "text/plain", "CHYBA\nNeznamy prikaz");
  }
}

// Hlavni funkce setup se zpracuje hned po startu cipu ESP32
void setup() {
  pinMode(2, OUTPUT);   // Zelene podsviceni
  pinMode(13, OUTPUT);  // Modre podsviceni
  // Nastartujeme serivou linku rychlosti 115200 b/s
  Serial.begin(115200);
  // Pripojime se k Wi-Fi a pote vypiseme do seriove linky IP adresu
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, heslo);
  Serial.printf("Pripojuji se k %s ", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Automaticke pripojeni pri ztrate Wi-Fi
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  // Vypiseme do seriove linky pro kontrolu LAN IP adresu mapy
  Serial.printf(" OK\nIP: %s\r\n", WiFi.localIP().toString());
  // Pro HTTP pozadavku / zavolame funkci httpDotaz
  server.on("/", httpDotaz);
  // Aktivujeme server
  server.begin();

  MDNS.begin("radar");
  MDNS.addService("http", "tcp", 80);
  // Nakonfigurujeme adresovatelene LED do vychozi zhasnute pozice
  // Nastavime 8bit jas na hodnotu 5
  // Nebude svitit zbytecne moc a vyniknou mene kontrastni barvy
  pixely.begin();
  pixely.setBrightness(jas);
  pixely.clear();
  pixely.show();

  memset(&historie, 0, 72 * sizeof(bod));  // Uvodni anulace pole s historii stavu bodu

  stahniData();  // Stahni data...
}

// Smycka loop se opakuje stale dokola
// a nastartuje se po zpracovani funkce setup
void loop() {
  // Vyridime pripadne TCP spojeni klientu se serverem
  server.handleClient();
  if (millis() - t > delay10) {
    t = millis();
    stahniData();
  }
  if (blikani_povoleno && (millis() - t2 > delay1)) {
    t2 = millis();
    for (uint8_t i = 0; i < 72; i++) {
      if (historie[i].blika) {
        if (tick) {
          pixely.setPixelColor(i, pixely.Color(historie[i].r1, historie[i].g1, historie[i].b1));
        } else {
          pixely.setPixelColor(i, pixely.Color(0, 0, 0));
        }
      }
    }
    pixely.show();
    tick = !tick;
  }
  // Pockame 2 ms (prenechame CPU pro ostatni ulohy na pozadi) a opakujeme
  delay(2);
}
