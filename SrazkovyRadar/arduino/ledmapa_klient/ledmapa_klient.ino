#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Nazev a heslo Wi-Fi
const char *ssid = "Nazev 2.4GHz Wi-Fi site";
const char *heslo = "Heslo Wi-Fi site";

// Objekt pro ovladani adresovatelnych RGB LED
// Je jich 72 a jsou v serii pripojene na GPIO pin 25
Adafruit_NeoPixel pixely(72, 25, NEO_GRB + NEO_KHZ800);
// HTTP server bezici na standardnim TCP portu 80
WebServer server(80);
// Pamet pro JSON s povely
// Alokujeme pro nej 10 000 B, co je hodne,
// ale melo by to stacit i pro jSON,
// ktery bude obsahovat instrukce pro vsech 72 RGB LED
StaticJsonDocument<10000> doc;

uint32_t t = 0;             // Refreshovaci timestamp
uint32_t delay10 = 600000;  // Prodleva mezi aktualizaci dat, 10 minut
uint8_t jas = 5;            // Vychozi jas

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
    pixely.clear();
    JsonArray mesta = doc["seznam"].as<JsonArray>();
    for (JsonObject mesto : mesta) {
      int id = mesto["id"];
      int r = mesto["r"];
      int g = mesto["g"];
      int b = mesto["b"];
      if (log) Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d\r\n", id, r, g, b);
      pixely.setPixelColor(id, pixely.Color(r, g, b));
    }
    pixely.show();
    return 0;
  }
}

// Stazeni radarovych dat z webu
void stahniData() {
  HTTPClient http;
  http.begin("http://kloboukuv.cloud/radarmapa/?chcu=posledni.json");
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
  } else if (server.hasArg("jas")) {
    server.send(200, "text/plain", "OK");
    jas = server.arg("jas").toInt();
    pixely.setBrightness(jas);
    pixely.show();
  }
  // Ve vsech ostatnich pripadech odpovime chybovym hlasenim
  else {
    server.send(200, "text/plain", "CHYBA\nNeznamy prikaz");
  }
}

// Hlavni funkce setup se zpracuje hned po startu cipu ESP32
void setup() {
  // Nastartujeme serivou linku rychlosti 115200 b/s
  Serial.begin(115200);
  // Pripojime se k Wi-Fi a pote vypiseme do seriove linky IP adresu
  WiFi.disconnect(); // Vynucene odpojeni; obcas pomuze, kdyz se cip po startu nechce prihlasit
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
  // Nakonfigurujeme adresovatelene LED do vychozi zhasnute pozice
  // Nastavime 8bit jas na hodnotu 5
  // Nebude svitit zbytecne moc a vyniknou mene kontrastni barvy
  pixely.begin();
  pixely.setBrightness(jas);
  pixely.clear();
  pixely.show();

  stahniData();  // Stahni data...
}

// Smycka loop se opakuje stale dokola
// a nastartuje se po zpracovani funkce setup
void loop() {
  // Vyridime pripadne TCP spojeni klientu se serverem
  server.handleClient();
  // Kazdych deset minut stahnu nova data
  if (millis() - t > delay10) {
    t = millis();
    stahniData();
  }
  // Pockame 2 ms (prenechame CPU pro ostatni ulohy na pozadi) a opakujeme
  delay(2);
}
