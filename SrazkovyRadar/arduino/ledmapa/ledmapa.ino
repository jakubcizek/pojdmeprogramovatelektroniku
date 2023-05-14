#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// Nazev a heslo Wi-Fi
const char *ssid = "Nazev 2.4GHz Wi-Fi";
const char *heslo = "WPA Heslo Wi-Fi";

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

// Tuto funkci HTTP server zavola v pripade HTTP GET/POST pzoadavku na korenovou cestu /
void httpDotaz(void) {
  // Pokud HTTP data obsahuji parametr mesta
  // predame jeho obsah JSON dekoderu
  if (server.hasArg("mesta")) {
    DeserializationError e = deserializeJson(doc, server.arg("mesta"));
    if(e){
      server.send(200, "text/plain", "CHYBA\nChyba v JSON");
      Serial.print("Spatne formatovany JSON");
    }
    // Pokud se nam podarilo dekodovat JSON,
    // zhasneme vsechny LED na mape a rozsvitime korektni barvou jen ty,
    // ktere jsou v JSON poli
    else{
      server.send(200, "text/plain", "OK");
      pixely.clear();
      JsonArray mesta = doc.as<JsonArray>();
      for(JsonObject mesto : mesta){
        int id = mesto["id"];
        int r = mesto["r"];
        int g = mesto["g"];
        int b = mesto["b"];
        Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d\r\n", id,r,g,b);
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
  else{
    server.send(200, "text/plain", "CHYBA\nNeznamy prikaz");
  }
}

// Hlavni funkce setup se zpracuje hned po startu cipu ESP32
void setup() {
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
