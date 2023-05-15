#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h> // https://github.com/adafruit/Adafruit_NeoPixel
#include <ArduinoJson.h> // https://arduinojson.org/

// Nazev a heslo Wi-Fi
const char *ssid = "Nazev 2.4GHz Wi-Fi site";
const char *heslo = "Heslo Wi-Fi site";

// Objekt pro ovladani adresovatelnych RGB LED
// Je jich 72 a jsou v serii pripojene na GPIO pin 25
Adafruit_NeoPixel pixely(72, 25, NEO_GRB + NEO_KHZ800);
// HTTP server bezici na standardnim TCP portu 80
WebServer server(80);
// Pamet pro JSON s povely
// Alokujeme pro nej 10 000 B, coz je fakt hodne,
// ale melo by to stacit i pro JSON, ktery bude 
// obsahovat instrukce pro vsech 72 RGB LED
// Mohli bychom JSON zjednodusit a usetrit bajty,
// anebo misto nej pouzit jiny format dat (BSON, CBOR...)
StaticJsonDocument<10000> doc;

// Tuto funkci HTTP server zavola v pripade HTTP GET/POST pozoadavku na korenovou cestu /
void httpDotaz(void) {
  // Pokud HTTP data obsahuji parametr mesta
  // predame jeho obsah JSON dekoderu
  if (server.hasArg("mesta")) {
    DeserializationError e = deserializeJson(doc, server.arg("mesta"));
    if (e) {
      if (e == DeserializationError::InvalidInput) {
        server.send(200, "text/plain", "CHYBA\nSpatny format JSON");
        Serial.print("Spatny format JSON");
      } else if (e == DeserializationError::NoMemory) {
        server.send(200, "text/plain", "CHYBA\nMalo pameti RAM pro JSON. Navys ji!");
        Serial.print("Malo pameti RAM pro JSON. Navys ji!");
      }
      else{
        server.send(200, "text/plain", "CHYBA\nNepodarilo se mi dekodovat jSON");
        Serial.print("Nepodarilo se mi dekodovat jSON");
      }
    }
    // Pokud se nam podarilo dekodovat JSON,
    // zhasneme vsechny LED na mape a rozsvitime korektni barvou jen ty,
    // ktere jsou v JSON poli
    else {
      server.send(200, "text/plain", "OK");
      pixely.clear();
      JsonArray mesta = doc.as<JsonArray>();
      for (JsonObject mesto : mesta) {
        int id = mesto["id"];
        int r = mesto["r"];
        int g = mesto["g"];
        int b = mesto["b"];
        Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d\r\n", id, r, g, b);
        pixely.setPixelColor(id, pixely.Color(r, g, b));
      }
      // Teprve ted vyrobime signal na GPIO pinu 25,
      // ktery nastavi svetlo na jednotlivych LED
      pixely.show();
    }
  }
  // Pokud jsme do mapy poslali jen HTTP GET/POST parametr smazat, mapa zhasne
  else if (server.hasArg("smazat")) {
    server.send(200, "text/plain", "OK");
    pixely.clear();
    pixely.show();
  }
  // Ve vsech ostatnich pripadech odpovime chybovym hlasenim
  else {
    server.send(200, "text/plain", "CHYBA\nNeznamy prikaz");
  }
}

// Hlavni funkce setup se zpracuje hned po startu cipu ESP32
void setup() {
  // Nastartujeme seriovou linku rychlosti 115200 b/s
  Serial.begin(115200);
  // Pripojime se k Wi-Fi a pote vypiseme do seriove linky IP adresu
  WiFi.disconnect(); // Vynuceny reset; obcas pomuze, kdyz se enchce cip pripojit
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
  Serial.printf(" OK\nIP: %s\r\n", WiFi.localIP().toString());
  // Pri HTTP pozadavku / zavolame funkci httpDotaz
  server.on("/", httpDotaz);
  // Aktivujeme server
  server.begin();
  // Nakonfigurujeme adresovatelene LED do vychozi zhasnute pozice
  // Nastavime 8bit jas na hodnotu 5
  // Nebude svitit zbytecne moc a vyniknou mene kontrastni barvy
  pixely.begin();
  pixely.setBrightness(5);
  pixely.clear();
  pixely.show();
}

// Smycka loop se opakuje stale dokola
// a nastartuje se po zpracovani funkce setup
void loop() {
  // Vyridime pripadne TCP spojeni klientu se serverem
  server.handleClient();
  // Pockame 2 ms (prenechame CPU pro ostatni ulohy na pozadi) a opakujeme
  delay(2);
}
