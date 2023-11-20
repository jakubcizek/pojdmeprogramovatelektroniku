// FIRMWARE TANKU S FLOWKAMEROU PAA5100JE
// Deska: ESP32-S3-DevKitC-1 s vestavenou RGB LED
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html
// U jinych desek ESP32 budete muset mirne upravit kod (ovladani RGB, piny aj.)

// Knihovny pro pro praci s Wi-Fi a HTTP serverem
// Soucast podpory cipu ESP32 pro Arduino
// https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// Knihovna pro praci s WebSockets
// https://github.com/Links2004/arduinoWebSockets
#include <WebSocketsServer.h>

// Knihovna pro praci s flowkamerou PAA5100JE
// https://github.com/jakubcizek/Optical_Flow_Sensors_Arduino
// Muj fork puvodni knihovny https://github.com/ralbra/Optical_Flow_Sensors_Arduino,
// ktera nema implementovanou metodu setOrientation, kterou pouzivame v kodu
#include <Optical_Flow_Sensor.h>

// Knihovna pro praci s gyroskopem MPU6050
// #include <Wire.h>
#include <MPU6050_tockn.h>

// HTML/JS stranky, favicona a PNG obrazek tanku
// Pole bajtu jsou konstanty, takze se na cipech ESP32
// budou automaticky nacitat rovnou z flashe a nebudou okupovat RAM
#include "html.h"
#include "js.h"
// Soubory bitmap jsme si prevedli na C pole
// na webu https://tomeko.net/online_tools/file_to_hex.php
#include "favicon.h"
#include "tank.h"

// Ovladaci piny motoru 1
#define M1A 4
#define M1B 5

// Ovladaci piny motoru 2
#define M2A 6
#define M2B 7

// Pokud mame k dispzioici vicejadrovy procesor,
// nami spustene asynchronni smycky FreeRTOS se
// spusti na druhem jadre
#if CONFIG_FREERTOS_UNICORE
#define CPU 0
#else
#define CPU 1
#endif

// SSID a heslo k 2.4GHz Wi-Fi
const char *ssid = "ssid";
const char *password = "heslo";

// Trida kamery PAA5100 pripojene na sbernici SPI
Optical_Flow_Sensor flowcamera(SS, PAA5100);

MPU6050 mpu6050(Wire);

// Trida HTTP serveru, ktery preda HTMl kod stranky
// TCP port 80
WebServer server(80);

// Trida WebSockets serveru, ktera se postara o streamovani
// dat mezi tankem a webovym prohlizecem
// TCP port 81
WebSocketsServer socket = WebSocketsServer(81);

uint8_t ws_clients_connected = 0;               // Pomocna promenna s poctem pripojenych WebSockets klientu
char orientations[4] = { 'N', 'E', 'S', 'W' };  // Pole s moznymi orinetacemi cipu
int8_t orientation = 0;                         // Index aktualni orientace
int rotation_time_ms = 900;                     // Doba nutna k otoceni tanku o 90 stupnu (pripad od pripadu)
bool xy_streaming = true;                       // Povoleni streamovani XY dat skrze WebSockets
int xy_time_ms = 100;                           // Doba pro mereni dostatecne zmeny XY

// Funkce pro jizdu dopredu
// Motory se toci stejne
void forward() {
  // Vestavena RGB LED se rozsviti bile
  neopixelWrite(RGB_BUILTIN, 255, 255, 255);
  digitalWrite(M1B, HIGH);
  digitalWrite(M1A, LOW);
  digitalWrite(M2B, HIGH);
  digitalWrite(M2A, LOW);
}

// Funkce pro jizdu dozadu
// Motory se toci stejne
void backward() {
  // Vestavena RGB LED se rozsviti cervene
  neopixelWrite(RGB_BUILTIN, 255, 0, 0);
  digitalWrite(M1A, HIGH);
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, HIGH);
  digitalWrite(M2B, LOW);
}

// Funkce pro zastaveni motoru
void stop() {
  // Vestavena RGB LED zhasne
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);
  digitalWrite(M1A, LOW);
  digitalWrite(M1B, LOW);
  digitalWrite(M2A, LOW);
  digitalWrite(M2B, LOW);
}

// Funkce pro otaceni se doleva
// Motory se toci protichudne
void left() {
  // Vestavena RGB LED se rozsviti modre
  neopixelWrite(RGB_BUILTIN, 0, 0, 255);
  digitalWrite(M1B, HIGH);
  digitalWrite(M1A, LOW);
  digitalWrite(M2A, HIGH);
  digitalWrite(M2B, LOW);
}

// Funkce pro otaceni se doprava
// Motory se toci protichudne
void right() {
  // Vestavena RGB LED se rozsviti zelene
  neopixelWrite(RGB_BUILTIN, 0, 255, 0);
  digitalWrite(M1A, HIGH);
  digitalWrite(M1B, LOW);
  digitalWrite(M2B, HIGH);
  digitalWrite(M2A, LOW);
}

// Funkce pro otoceni tanku o 90 stupnu
// po, nebo proti smeru hodinovych rucicek
// Nemame IMU/gyro, takze nemame zpetnou vazbu otoceni
// a otoceni o 90 stupnu odhadujeme jen dobou v ms
void turn_90(bool clock_wise) {
  xy_streaming = false;  // Pozastavime zasilani XY zmen skrze WebSocket
  // Zmerime aktualni uhel otoceni tanku
  float angle_start = getSmoothAngleZ(true);
  float angle_diff = 0;
  // Pokud se mame otacet po smeru hodinovych rucicek,
  // aktivujeme otaceni doprava
  if (clock_wise) {
    right();
    orientation++;
    if (orientation > 3) orientation = 0;
    // Anebo naopak doleva
  } else {
    left();
    orientation--;
    if (orientation < 0) orientation = 3;
  }
  // V tuto chvili se tank otaci, a my blokujeme program,
  // dokud gyroskop nezmeri absolutni rozdil oproti vychozi orientaci alespon 90 stupnu
  // Gyroskop MPU6050 neni nejpresnejsi, ale je to nejjednodussi reseni, ktere je alespon trosku spolehlivy
  while (angle_diff < 90) {
    mpu6050.update();
    float angle = getSmoothAngleZ(true);
    angle_diff = fabs(angle_start - angle);
    Serial.printf("now=%.2f\tdiff=%.2f\tstart=%.2f\r\n", angle, angle_diff, angle_start);
    delay(100);
  }
  // Tank se otocil alespon o 90 stupnu,
  // a tak zastavime motory a nastavime na flowkamere novou orientaci os XY
  stop();
  flowcamera.setOrientation(orientations[orientation]);
  // Skrze WebSocket posleme zpravu pripojenym klientum,
  // ze je otaceni dokoncene a potvrdime novou pravouhlou orientaci 0-3 (pseudo N/E/S/W)
  char msg[20];
  sprintf(msg, "SYS:ORIENTATION,%c", orientations[orientation]);
  if (ws_clients_connected > 0) {
    socket.broadcastTXT(msg);
  }
  // Nechame flowkameru jeste trikrat zmerit XY,
  // abychom v klientske aplikaci ignorovali sum pri otaceni
  // a opet zacneme streamovat XY data klientum
  delay(xy_time_ms * 3);
  xy_streaming = true;
}

// Funkce pro vypocet shlazeneho uhlu otoceni
// pomoci aritmetickeho prumeru s 10 vzorky
float getSmoothAngleZ(bool absolute) {
  const uint16_t steps = 10;
  float angle = 0;
  for (uint16_t i = 0; i < steps; i++) {
    mpu6050.update();
    angle += mpu6050.getAngleZ();
  }
  if (absolute) {
    return fabs(angle / (float)steps);
  } else {
    return angle / (float)steps;
  }
}

// Pri udalosti na WebSockets serveru
// Pripojeni/odpojeni klienta, prichozi data od klienta...
void onWebSocketsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    // Pripojil se novy klient
    case WStype_DISCONNECTED:
      ws_clients_connected--;
      Serial.println("WebSocket klient odpojen");
      break;

    // Odpojil se klinet
    case WStype_CONNECTED:
      ws_clients_connected++;
      Serial.println("WebSocket klient pripojen");
      break;

    // Klient posila textova data
    case WStype_TEXT:
      Serial.printf("WS klient: %s\r\n", payload);

      // Vsechny prikazy maji alespon 3 znaky
      if (length >= 3) {

        // Pokud posila CL0, nebo CL1 ,vypneme/zapneme LED flowkamery
        if (payload[0] == 'C' && payload[1] == 'L') {
          if (payload[2] == '0') {
            flowcamera.setLed(false);
          } else if (payload[2] == '1') {
            flowcamera.setLed(true);
          }
        }

        // Pokud posila TRcislo, jedna se o dobu v ms pro otoceni o 90 stupnu
        // Doba se muze menit podle povrchu/stavu akumulatoru, takze je dobre ji
        // pripadne urpavit za chodu - bez kvalitni IMU/gyro to neni idealni
        else if (payload[0] == 'T' && payload[1] == 'R') {
          rotation_time_ms = atoi((char *)(payload + 2));
        }

        // Pokud posila STOP, zastavime motory
        else if (strcmp((char *)payload, "STOP") == 0) {
          stop();
        }

        // Pokud posila FORWARD, spustime motory pro jizdu dopredu
        else if (strcmp((char *)payload, "FORWARD") == 0) {
          forward();
        }

        // Pokud posila BACKWARD, spustime motory pro jizdu dozadu
        else if (strcmp((char *)payload, "BACKWARD") == 0) {
          backward();
        }

        // Pokud posila LEFT, spustime oteceni o 90 stupnu doleva
        else if (strcmp((char *)payload, "LEFT") == 0) {
          turn_90(false);  // Proti smeru hodinovych rucicek
        }

        // Pokud posila RIGHT, spustime oteceni o 90 stupnu doprava
        else if (strcmp((char *)payload, "RIGHT") == 0) {
          turn_90(true);  // Po smeru hodinovych rucicek
        }

        // Pokud posila ORI_RESET, resetujeme orientaci flowkamery do vychozi pozice
        else if (strcmp((char *)payload, "ORI_RESET") == 0) {
          flowcamera.setOrientation('N');
        }
      }
      break;
  }
}

// Asynchronni FreeRTOS smycka pro cteni dat z flowkamery PAA5100
void loop_flowcamera(void *args) {
  while (true) {
    // Pomocne promenne pro relativni zmenu X a Y od posledniho mereni
    int16_t delta_x, delta_y;
    // Precteme hodnty zmen v osach X a Y
    flowcamera.readMotionCount(&delta_x, &delta_y);
    // Pokud jse pripojeny nejaky WS klient a je povolene zasilani,
    // odesleme souradnice jako zpravu ve formatu XY:X,Y
    if (ws_clients_connected > 0 && xy_streaming) {
      char xy[15];
      sprintf(xy, "XY:%d,%d", delta_x, delta_y);
      socket.broadcastTXT(xy);
    }
    // Pockame 100 ms, abychom dokazali zachytit
    // dostatecne velkou relativni zmenu a vse zopakujeme
    delay(xy_time_ms);
  }
}

// Hlavni funkce setup se spusti hned po startu
void setup() {
  // Nastartujeme seriovou linku do PC rychlsoti 115200 b/s
  Serial.begin(115200);
  delay(2000);

  // Ovldaci piny dualniho H-musrtku/motoroveho driveru
  pinMode(M1A, OUTPUT);
  pinMode(M1B, OUTPUT);
  pinMode(M2A, OUTPUT);
  pinMode(M2B, OUTPUT);

  // Kontrolni vypis definice pinu sbernice SPI
  // Hodi se, pokud nemame jasno
  Serial.printf("MOSI=%d, MISO=%d, SCK=%d SS=%d\r\n", MOSI, MISO, SCK, SS);
  Serial.printf("SDA=%d, SCL=%d\r\n", SDA, SCL);

  // Nastartujeme kameru
  flowcamera.begin();
  flowcamera.setOrientation('N');

  Wire.begin();
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true);
  Serial.println("");

  // Pripojime se k Wi-Fi
  // Pokud to bude trvat dele nez 60 sekund, restartuje se CPU
  Serial.print("Pripojuji se k Wi-Fi ");
  WiFi.mode(WIFI_STA);
  delay(1000);
  WiFi.begin(ssid, password);
  uint32_t zacatek = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if ((millis() - zacatek) > 60000) {
      Serial.println(" CHYBA\r\nPrihlasuji se prilis dlouho, restartuji MCU!");
      delay(1000);
      ESP.restart();
    }
  }
  // Po uspesnem pripojeni k WI-Fi vypiseme do seriove linky lokalni IP adresu,
  // kterou nam pridelil DHCP server
  Serial.printf(" %s\r\n", WiFi.localIP().toString().c_str());

  // Rekneme siti, ze jsme k dispozici na lokalni DNS tank.local
  MDNS.begin("tank");
  MDNS.addService("http", "tcp", 80);

  // Nastartujeme WebSocket server
  socket.begin();
  socket.onEvent(onWebSocketsEvent);

  // Nastartujeme HTTP server a nastavime, co se ma stat pri pozadavku  GET na korenovy adresar
  // V takovem pripade posleme klientovi primo z flashe konstatu html s HTML kodem stranky
  // Konstanta html se nachazi v hlavickovem souboru html.h
  server.on("/", []() {
    server.send_P(200, "text/html", html);
  });

  // Pri pozadavku /app.js posleme konstantu js, ktera se nachazi v hlavickovem souboru js.h
  server.on("/app.js", []() {
    server.send_P(200, "text/javascript", js);
  });

  // Pri pozadavku /favicon.ico posleme konstantu favicon z hlavickoveho souboru favicon.h
  server.on("/favicon.ico", []() {
    server.send_P(200, "image/x-icon", favicon, sizeof(favicon));
  });

  // Pri pzoadavku /tank.png posleme konstantu tank z hlavickoveho souboru tank.h
  server.on("/tank.png", []() {
    server.send_P(200, "image/png", tank, sizeof(tank));
  });

  // Nastartujeme HTTP server
  server.begin();

  // Nastartujeme asynchronni FreeRTOS ulohu loop_flowkamera na CPU jadre definovanem v makru CPU,
  // ktera bude v nekonecne smycce cist data z flowkamery a posilat je pripojenym WebSockets klientum
  // Dokumentace: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_additions.html?highlight=xtaskcreatepinnedtocore#_CPPv423xTaskCreatePinnedToCore14TaskFunction_tPCKcK8uint32_tPCv11UBaseType_tPC12TaskHandle_tK10BaseType_t
  xTaskCreatePinnedToCore(loop_flowcamera, "loop_flowcamera", 4096, NULL, 1, NULL, CPU);
}

// Nekonecna smycka loop v prostredi Arduino
void loop() {
  server.handleClient();  // Vyrid pripadne pozadavky HTTP serveru
  socket.loop();          // Vyrid pripadne pozadvaky WebSocket serveru
  delay(2);               // Pockej 2 ms a opakuj
}
