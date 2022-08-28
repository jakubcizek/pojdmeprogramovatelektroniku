/*
   Firmware pro Mriju, Arduino a mikrokontroler ESP32
   Po pripojeni napajeni se cip:
   1) Spoji s 128x32 OLED displejem s I2C radicem SSD1306
   2) Spoji s I2C gyroskopem MPU6050
   3) Pripoji se k preddefinovane Wi-Fi
   4) Na TCP portu 80 nastartuje websocketovy server

   Ve smycce bude cekat na pripojeni klienta
   Pokud k tomu dojde, zacne mu posilat uhly natoceni gyroskopu
   jako prosty text ve formatu X.XXX,Y.YYY,Z.ZZZ

   Pouzite externi knihovny pro Arduino:
   1) https://github.com/tockn/MPU6050_tockn
   2) https://github.com/Links2004/arduinoWebSockets
   3) https://github.com/adafruit/Adafruit_SSD1306
*/

#include <WiFi.h>              // Wi-Fi komunikace
#include <ESPmDNS.h>           // LAN MDNS
#include <esp_wifi.h>          // Vynuceni plneho vykonu Wi-Fi
#include <WebSocketsServer.h>  // Websockets
#include <Wire.h>              // I2C
#include <MPU6050_tockn.h>     // Gyroskop MPU6050
#include <Adafruit_SSD1306.h>  // OLED displej

// SSID a heslo Wi-Fi,
// ke ktere se bude pripojovat fyzicka Mrija
// Pro jednoduchost natvrdo soucasti firmwaru
const char *ssid = "nazev-wifi-site";
const char *password = "heslo-k-wifi";

// Trida gyroskopu
MPU6050 mpu6050(Wire);

// Trida websocketoveho serveru na TCP portu 80
WebSocketsServer websocket = WebSocketsServer(80);

// Trida SSD1306 I2C OLED displeje 128x32 px
Adafruit_SSD1306 display(128, 32, &Wire, -1);

// Pomocne promenne pro aktualizaci dat z gyroskopu
// a zasilani skrze websocket kazdych x milisekund
uint32_t casOdeslanychDat = 0;
uint32_t prodlevaMeziOdeslanim = 100; // ms

// Funkce setup se spusti na zacatku programu
void setup() {
  // Nastartovani seriove linky
  Serial.begin(115200);
  delay(1000);

  // Nastartuj sbernici I2C
  Wire.begin();

  // Nastartuj displej
  Serial.print("Startuji displej na I2C adrese 0x3C... ");
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OK");
  }
  else Serial.println("CHYBA");

  // Nastartuj gyroskop
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Gyroskop... ");
  display.display();
  Serial.print("Startuji gyroskop MPU6050... ");
  mpu6050.begin(); // Knihovna pri startu nevraci zadnou hodnotu a nelze overit, jestli spojeni funguje
  Serial.println("OK");
  display.println("OK");
  display.display();
  delay(2000);

  // Nastartuj Wi-Fi
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Wifi...");
  display.display();
  Serial.print("Startuji Wi-Fi...");
  WiFi.mode(WIFI_STA);
  // Vynuceni vysokeho vykonu WI-FI, ale tim padem i vyssi spotreby
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#_CPPv415esp_wifi_set_ps14wifi_ps_type_t
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.begin(ssid, password);
  // Konfigurace MDNS
  // Na podporovanych platformach bude
  // Mrija dostupna i na LAN domene mrija.local
  MDNS.begin("mrija");
  MDNS.addService("http", "tcp", 80);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK");
  Serial.printf("IP: %s\r\n", WiFi.localIP().toString().c_str());
  display.println(" OK");
  display.display();
  delay(2000);

  // Namir Mriju cumakem k displeji
  // Provedu kalibraci gyroskopu
  // Natoceni cumakem k displeji je klicove,
  // jinak bude Mrija na monitoru natocena
  // pod spatnym uhlem v rovine (osa Z/boceni)
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Namir Mriju");
  display.println("k displeji");
  display.display();
  Serial.println("Namir zarizeni v rovine na displej");
  delay(5000);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Kalibruji... ");
  display.display();
  Serial.println("Kalibruji gyroskop... ");
  mpu6050.calcGyroOffsets(true);
  display.print("OK");
  display.display();
  Serial.println("");
  delay(2000);

  // Nastartuj websocketovy server na stanovenem TCP portu
  // Pri udalosti zavola funkci websocketUdalost
  websocket.begin();
  websocket.onEvent(websocketUdalost);

  // Konfigurace je hotova,
  // a tak vypiseme an displej IP adresu Mriji
  // Pote se uz spusti smycka loop
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("IP adresa letadla:");
  display.println(WiFi.localIP().toString());
  display.display();
}

// Funkce loop se opakuje stale dokola
void loop() {
  // Zpracuj ulohy websocketoveho serveru
  websocket.loop();
  // Pokud se pripojil nejaky websocketovy klient
  // a zaroven uplynulo 100 ms od posledniho odeslani dat,
  // ziskej nove hodnoty z gyroskopu a odesli je vsem
  // pripojenym klientum a pro kontrolu vypis do seriove linky
  if (websocket.connectedClients(false) > 0 && millis() - casOdeslanychDat > prodlevaMeziOdeslanim) {
    casOdeslanychDat = millis();
    mpu6050.update();
    char zprava[100];
    sprintf(zprava, "%f,%f,%f", mpu6050.getAngleX(), mpu6050.getAngleY(), mpu6050.getAngleZ());
    websocket.broadcastTXT(zprava, strlen(zprava));
    Serial.println(zprava);
  }
}

// Tuto funkci zavola websocketovy server, pokud se neco stane
// Registruji udalost pripojeni/odpojeni klientu a prijem textovych dat
void websocketUdalost(uint8_t id, WStype_t typ, uint8_t *data, size_t length) {
  IPAddress ip = websocket.remoteIP(id);
  switch (typ) {
    // Klient se odpojil
    case WStype_DISCONNECTED:
      Serial.printf("Klient %u (%s) se odpojil\r\n", id, ip.toString().c_str());
      break;
    // Klient se pripojil, vypisu jeho IP adresu
    case WStype_CONNECTED:
      Serial.printf("Klient %u se pripojil z IP adresy %s\r\n", id, ip.toString().c_str());
      websocket.sendTXT(id, "#Mrija Websocket Server te vita!");
      break;
    // Textova ASCII data od klienta
    // Jen je vypisu do seriove linky, mohli bychom je ale pouzit
    // treba pro konfiguraci
    case WStype_TEXT:
      Serial.printf("Zprava od klienta %u (%s): %s\n", id, ip.toString().c_str(), data);
      break;
  }
}
