/*

Použitý hardware:

Základní deska ESP32-S3-DevKitC-1
https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html
(nebo jakákoliv jiná s čipem ESP32, ale pak si budete muset upravit čísla pinů a nastavování vestavěné RGB LED)

Motorový ovladač pro čtyři DC motory s digitální logikou shodnou s duálními H-můstky DRV8833
https://www.ti.com/lit/ds/symlink/drv8833.pdf

Instalace podpory pro čipy ESP32 ve vývojovém prostředí Arduino
https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html

*/

// Knihovny pro práci s Wi-Fi, mDNS, HTTP serverem a Websockety
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
// Pro práci s Websockety používám tuto knihovnu z GitHubu
// https://github.com/Links2004/arduinoWebSockets
// Je tedy třeba ji ručně doinstalovat
#include <WebSocketsServer.h>

// HTML kód stránky vozidla, Javascript a favikona
#include "html.h"
#include "js.h"
#include "favicon.h"

// GPIO piny na čipu ESP32-S3, kterými budeme ovládat motory
// Upravte dle libosti a možností vlastní desky

// MOTOR 0 - LEVÝ PŘEDNÍ
#define M1A 4
#define M1B 5

// MOTOR 1 - PRAVÝ PŘEDNÍ
#define M2A 6
#define M2B 7

// MOTOR 2 - LEVÝ ZADNÍ
#define M3A 15
#define M3B 16

// MOTOR 3 - PRAVÝ ZADNÍ
#define M4A 17
#define M4B 18

// SSID a heslo k 2,4GHz Wi-Fi
const char *ssid = "...";
const char *password = "...";

// Objekt HTTP serveru, který poběží na standardním TCP portu 80
WebServer server(80);

// Objekt Websocket serveru, který poběží na TCP portu 81
WebSocketsServer socket = WebSocketsServer(81);

// Pomocná proměnná s počtem připojených websocketových klientů
uint8_t ws_clients_connected = 0;

// PWM rychlost vozidla
// Předpokládáme, že použité H-můstky (DRV8833) podporují PWM signál
uint8_t speed = 255;

// Struktura motoru s ovládacími signály A a B
typedef struct {
  uint8_t A;
  uint8_t B;
} MOTOR;

// Vytvoříme pole čtyř motorů
MOTOR motors[4];

// GPIO konfigurace motorů
// Projdeme pole motorů a jejich signály napojíme na odpovídající piny GPIO
// Všechny GPIO poté nastavíme na výstup
void setupMotorsGPIO() {
  motors[0].A = M1A;
  motors[0].B = M1B;

  motors[1].A = M2A;
  motors[1].B = M2B;

  motors[2].A = M3A;
  motors[2].B = M3B;

  motors[3].A = M4A;
  motors[3].B = M4B;

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(motors[i].A, OUTPUT);
    pinMode(motors[i].B, OUTPUT);
  }
}

// Funkce pro nastavení stavu dílčího motoru
// Stavem může být:
// 'F' pro otáčení ve směru vozidla (forward)
// 'B' pro otáčení proti směru vozidla (backward)
// 'X' pro zastavení otáčení motoru
// Kombinace signálů odpovídají logice H-můstku DRV8833
// a adekvátnímu spojení jeho výstupů s napájením motorů
void setMotor(uint8_t motor, char mode) {
  if (mode == 'F') {
    analogWrite(motors[motor].A, speed);
    analogWrite(motors[motor].B, 0);
  } else if (mode == 'B') {
    analogWrite(motors[motor].A, 0);
    analogWrite(motors[motor].B, speed);
  } else if (mode == 'X') {
    analogWrite(motors[motor].A, 0);
    analogWrite(motors[motor].B, 0);
  }
}


// Komplexní funkce pro jízdu dopředu
// Všechny motory se otáčejí vpřed
// Vestavěná RGB LED se rozsvítí bíle (pouze podporované desky!)
void forward() {
  neopixelWrite(RGB_BUILTIN, 255, 255, 255);
  setMotor(0, 'F');
  setMotor(1, 'F');
  setMotor(2, 'F');
  setMotor(3, 'F');
}

// Komplexní funkce pro jízdu dozadu
// Všechny motory se otáčejí dozadu
// Vestavěná RGB LED se rozsvítí červeně (pouze podporované desky!)
void backward() {
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);
  setMotor(0, 'B');
  setMotor(1, 'B');
  setMotor(2, 'B');
  setMotor(3, 'B');
}

// Komplexní funkce pro zastavení všech motorů
// Vestavěná RGB LED zhasne (pouze podporované desky!)
void stop() {
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);
  for (uint8_t i = 0; i < 4; i++) {
    setMotor(i, 'X');
  }
}

// Komplexní funkce pro jízdu vlevo
// Motory se roztočí podle schématu v článku
// Jízda vlevo je výsledkem součtu vektorů otáčejících se gumových válečků kola mecanuum
// Vestavěná RGB LED se rozsvítí zeleně (pouze podporované desky!)
void left() {
  neopixelWrite(RGB_BUILTIN, 0, 0, 255);
  setMotor(0, 'F');
  setMotor(1, 'B');
  setMotor(2, 'B');
  setMotor(3, 'F');
}

// Komplexní funkce pro jízdu vpravo
// Motory se roztočí podle schématu v článku
// Jízda vpravo je výsledkem součtu vektorů otáčejících se gumových válečků kola mecanuum
// Vestavěná RGB LED se rozsvítí zeleně (pouze podporované desky!)
void right() {
  // Vestavena RGB LED se rozsvítí zeleně
  neopixelWrite(RGB_BUILTIN, 0, 255, 0);
  setMotor(0, 'B');
  setMotor(1, 'F');
  setMotor(2, 'F');
  setMotor(3, 'B');
}

/*

Stejným způsobem a podle schématů v článku můžete doprogramovat
pohybové kombinace pro diagonální jízdu, komplexní otáčení jako na pásovém robotu aj. 

*/


// Funkce, která se volá při Websocket události
// Může to být připojení/odpojení klienta, příchozí data atp.
void onWebSocketsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    // Připojil se nový klient
    case WStype_DISCONNECTED:
      ws_clients_connected--;
      Serial.println("WebSocket klient odpojen");
      break;

    // Klient se odpojil
    case WStype_CONNECTED:
      ws_clients_connected++;
      Serial.println("WebSocket klient pripojen");
      break;

    // Klient posílá textová data
    // Může to být FORWARD, BACKWARD, LEFT a RIGHT pro pravoúhlou jízdu
    // A také Pčíslo, kde číslo představuje hodnotu 0-255 pro nastavení PWM výkonu motorů
    // Použité motory ze stavebnice budou při hmotnosti celkého vozidla a napájení 8,4 V
    // spolehlivě pracovat při PWM výkonu zhruba od hodnoty 100. Při nižších hodnotách
    // už bude napětí natolik snížené, že budou motory jen pískat. Záleží ale i na odporu povrchu
    case WStype_TEXT:
      Serial.printf("WS klient: %s\r\n", payload);

      // Všechny povely mají délku alespoň 2 znaky
      if (length >= 2) {

        // Pokud dorazilo Sxxx, kde xxx je číslo,
        // nastavíme PWM výkon
        if (payload[0] == 'P') {
          speed = atoi((char *)(payload + 1));
          Serial.printf("Zmena rychlosti na %d\r\n", speed);
        }

        // Pokud dorazilo STOP, zastavíme motory
        else if (strcmp((char *)payload, "STOP") == 0) {
          stop();
        }

        // Pokud dorazilo FORWARD, vozidlo se rozjede dopředu
        else if (strcmp((char *)payload, "FORWARD") == 0) {
          forward();
        }

        // Pokud dorazilo BACKWARD, vozidlo se rozjede dozadu
        else if (strcmp((char *)payload, "BACKWARD") == 0) {
          backward();
        }

        // Pokud dorazilo LEFT, vozidlo se rozjede doleva
        else if (strcmp((char *)payload, "LEFT") == 0) {
          left();
        }

        // Pokud dorazilo RIGHT, vozidlo se rozjede doprava
        else if (strcmp((char *)payload, "RIGHT") == 0) {
          right();
        }
      }
      break;
  }
}


// Hlavní funkce setup se spustí hned na začátku programu / po připojení napájení
void setup() {
  // Nastartujeme sériovou linku do PC rychlostí 115200 b/s a 2 s počkáme
  // Jen pro účelý vývoje a testování motorů, pokud bude robot připojený k PC skrze USB
  Serial.begin(115200);
  delay(2000);

  // Nastavíme GPIO motorů a uvedeme je do stavu STOP
  // Důležité pro případy, kdy řídící čip nastavuje na zvolených GPIO odlišné výchozí stavy
  setupMotorsGPIO();
  stop();

  // Připojíme se k předdefinované Wi-Fi
  // Pokud bude připojování trvat déle než 60 sekund,
  // restartujeme čip a začneme znovu
  Serial.print("Pripojuji se k Wi-Fi ");
  WiFi.mode(WIFI_STA);
  delay(1000);
  WiFi.begin(ssid, password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if ((millis() - start) > 60000) {
      Serial.println(" CHYBA\r\nPrihlasuji se prilis dlouho, restartuji MCU!");
      delay(1000);
      ESP.restart();
    }
  }

  // Po úspěšném přihlášení vypíšeme do sériové linky přidělenou
  // místní IP od DHCP serveru. Na této adrese bude poslouchat webový server
  Serial.printf(" %s\r\n", WiFi.localIP().toString().c_str());

  // Pomocí technologie mDNS řekneme síti, že bude náš webový server
  // dostupný na lokální adrese http://vozidlo.local
  // Pozor, mDNS nepodporuje každý operační systém (tetsováno na Windows 11)
  MDNS.begin("vozidlo");
  MDNS.addService("http", "tcp", 80);

  // Nastartujeme Websocket
  // Při událsoti se zavolá funkce onWebSocketsEvent
  // Webscoket je rychlé a obousměrné a udržované spojení mezi webovým prohlížečem a vozidlem
  // Budeme skrze něj do vozdila posílat povely pro jízdu
  socket.begin();
  socket.onEvent(onWebSocketsEvent);

  // Nastartujeme HTTP server a nastavíme, co se má stát při požadavku GET na kořenový adresář
  // V takovém případě pošleme klientovi přímo z flashové paměti konstatu html s HTML kodem stránky
  // Konstanta html se nachází v hlavičkovém souboru html.h
  server.on("/", []() {
    server.send_P(200, "text/html", html);
  });

  // Pri požadavku /app.js pošleme konstantu js, která se nacházi v hlavičkovém souboru js.h
  server.on("/app.js", []() {
    server.send_P(200, "text/javascript", js);
  });

  // Při požadavku /favicon.ico pošleme konstantu favicon z hlavičkového souboru favicon.h
  server.on("/favicon.ico", []() {
    server.send_P(200, "image/x-icon", favicon, sizeof(favicon));
  });

  // Nastartujeme HTTP server
  server.begin();
}

// Nekonečná smyčka loop se spustí po zpracování funkce setup
void loop() {

  // Pokud skrze sériovou linku pošleme znaky 1-9,b,l,r,x,d,
  // roztočí se motory kýženým směrem - slouží pro testování a ladění
  while (Serial.available()) {
    char cmd = (char)Serial.read();
    Serial.printf("Prichozi znak: %c\r\n", cmd);

    // Komplexní pohyby
    if (cmd == 'f') forward();
    else if (cmd == 'b') backward();
    else if (cmd == 'l') left();
    else if (cmd == 'r') right();
    else if (cmd == 'x') stop();

    // Časová prodleva
    // Takže "fddx" spustí na 2 sekundy jízdu vpřed
    else if (cmd == 'd') delay(1000);

    // Ladění otáčení jednotlivých motorů 0-4
    else if (cmd == '1') setMotor(0, 'F');
    else if (cmd == '2') setMotor(1, 'F');
    else if (cmd == '3') setMotor(2, 'F');
    else if (cmd == '4') setMotor(3, 'F');

    else if (cmd == '5') setMotor(0, 'B');
    else if (cmd == '6') setMotor(1, 'B');
    else if (cmd == '7') setMotor(2, 'B');
    else if (cmd == '8') setMotor(3, 'B');
  }

  // Vyřídíme případné požadavky HTTP serveru a Websocketu
  // a smyčka začíná nanovo
  server.handleClient();
  socket.loop();
  delay(1);
}
