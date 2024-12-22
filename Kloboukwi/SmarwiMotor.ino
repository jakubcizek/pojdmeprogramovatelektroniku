#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <SPI.h> 
#include <EEPROM.h>
#include "SparkFunL6470.h" 
#include "web.h"
#include "eeprom_mgmt.h"

// https://github.com/EinarArnason/ArduinoQueue
#include <ArduinoQueue.h>

// GPIO na ESP8266
#define CS 15 
#define MOSI 13             
#define MISO 12              
#define SCK 14              
#define dSPIN_RESET 16       
#define dSPIN_BUSYN 2        

#define LED 1
#define IR_STOP 3
#define IR_GUIDE 5
#define BUTTON 0

// Stavy pro pohybovy asynchronni state machine:

// Chybove stavy
#define STATE_UNKNOWN 0xff
#define STATE_NO_GUIDE 0xfe

// Stavy pro zavreni
#define STATE_CLOSE 1
#define STATE_CLOSING 2
#define STATE_CLOSED 3

// Stavy pro otevreni
#define STATE_OPEN 4
#define STATE_OPENING 5
#define STATE_OPENED 6

// Stavy pro otevreni o kousek
#define STATE_OPEN_BY_STEP 7
#define STATE_OPENING_BY_STEP 8
#define STATE_OPENED_BY_STEP 9

// Stavy pro zavreni o kousek
#define STATE_CLOSE_BY_STEP 10
#define STATE_CLOSING_BY_STEP 11
#define STATE_CLOSED_BY_STEP 12

// Stavy pro zavreni podle IR cidla
#define STATE_CLOSE_TO_IR 13
#define STATE_CLOSING_TO_IR 14
#define STATE_CLOSED_TO_IR 15

// Stavy pro zavreni podle IR cidla a reset pozice
#define STATE_CLOSE_TO_IR_AND_RESET_POSITION 16
#define STATE_CLOSING_TO_IR_AND_RESET_POSITION 17
#define STATE_CLOSED_TO_IR_AND_RESETED_POSITION 18

// Stavy pro posun na konkretni pozici
#define STATE_MOVE_TO 19
#define STATE_MOVING_TO 20
#define STATE_MOVED_TO 21

// Vychozi hodnoty
#define DEFAULT_OPEN_POSITION 500
#define DEFAULT_OPEN_CLOSE_BIT_CHANGE 1000
#define DEFAULT_MAX_SPEED 290

// Fronta na max dvacet povelu, ktere se zpracuji jeden za druhym 
ArduinoQueue<uint8_t> state_queue(20);

// Pomocne promenne
uint8_t state = STATE_UNKNOWN;
bool state_ready = true;
bool ir_force_check = false;
uint32_t pos = 0;
uint32_t target = 0;
uint32_t pos_check = 0;
bool movement = false;
bool lock = true;
uint8_t ws_connected = 0;
int ir_stop;
int ir_guide;

uint32_t open_position = DEFAULT_OPEN_POSITION;
uint32_t close_position = DEFAULT_OPEN_POSITION - 1000;
uint32_t open_close_bit_change = DEFAULT_OPEN_CLOSE_BIT_CHANGE;

uint8_t motor_step_resolution = dSPIN_STEP_SEL_1_64;
uint8_t motor_overcurrent_detection = dSPIN_OCD_TH_6000mA;
uint32_t motor_max_speed = DEFAULT_MAX_SPEED;

// Nastaveni Wi-Fi
const char* ssid = "";
const char* password = "";

// Udaj s datema  casem kompilace
const char* buildDateTime = "BUILD " __DATE__ " " __TIME__;

// WebSocket server bezi na portu 81
WebSocketsServer webSocket = WebSocketsServer(81);

// HTTP server bezi na standardnim portu 80
ESP8266WebServer server(80);

// Deklarace funkci
void handleRoot();
void handleNotFound();
void handleMovement();
void handleStatus();
void setSafeState(uint8_t newstate);
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void notify();
void notify_value(const char* tag, uint32_t value);
void updateMotorStateMachine();
void check_ir_guide_change();
void check_ir_stop_change();
void check_guide_ready();
void check_notify_scheduler();

// HTTP root stranka
void handleRoot() {
  server.send(200, "text/html", html);
}

// HTTP pohybove akce:
void handleMovement() {
  if (server.hasArg("target")) {
    if (server.arg("target") == "open") {
      server.send(200, "application/json", "{\"code\": 1}");
      setSafeState(STATE_OPEN);
    } else if (server.arg("target") == "close") {
      server.send(200, "application/json", "{\"code\": 1}");
      setSafeState(STATE_CLOSE);
    } else if (server.arg("target") == "open-bit") {
      server.send(200, "application/json", "{\"code\": 1}");
      setSafeState(STATE_OPEN_BY_STEP);
    } else if (server.arg("target") == "close-bit") {
      server.send(200, "application/json", "{\"code\": 1}");
      setSafeState(STATE_CLOSE_BY_STEP);
    } else if (server.arg("target") == "close-bit") {
      server.send(200, "application/json", "{\"code\": 1}");
      setSafeState(STATE_CLOSE_BY_STEP);
    } else if (server.arg("target") == "position") {
      target = server.arg("value").toInt();
      setSafeState(STATE_MOVE_TO);
      server.send(200, "application/json", "{\"code\": 1}");
    } else {
      server.send(200, "application/json", "{\"code\": 0}");
    }
  } else {
    server.send(200, "application/json", "{\"code\": 0}");
  }
}

// HTTP nastaveni
void handleSet() {
  if (server.hasArg("key")) {
    if (server.arg("key") == "bit") {
      server.send(200, "application/json", "{\"code\": 1}");
      open_close_bit_change = (uint32_t)server.arg("value").toInt();
      setU32EEPROM(6, open_close_bit_change);
      notify();
    } else if (server.arg("key") == "unlock") {
      server.send(200, "application/json", "{\"code\": 1}");
      dSPIN_Xfer(dSPIN_HARD_HIZ);
      lock = false;
      notify();
    } else if (server.arg("key") == "lock") {
      server.send(200, "application/json", "{\"code\": 1}");
      dSPIN_Xfer(dSPIN_HARD_STOP);
      lock = true;
      setSafeState(STATE_CLOSE_TO_IR_AND_RESET_POSITION);
      target = pos;
      setSafeState(STATE_MOVE_TO);
      notify();
    }
  }
}

// HTTP status
void handleStatus() {
  pos = dSPIN_GetParam(dSPIN_ABS_POS);
  char msg[2000];
  int irstop = digitalRead(IR_STOP);
  int irguide = digitalRead(IR_GUIDE);
  sprintf(msg, "{\"code\": 1, \"state\": %d, \"pos\": %d, \"irstop\": %d, \"irguide\": %d,  \"open_position\": %d, \"close_position\": %d, \"open_close_bit_change\": %d, \"motor_max_speed\": %d}", state, pos, irstop, irguide, open_position, close_position, open_close_bit_change, motor_max_speed);
  server.send(200, "application/json", msg);
}

// Arduino main
void setup() {
  // NActeni ulozenych hodnot z virtualni EEPROM, anebo reset
  EEPROM.begin(EEPROM_SIZE);
  if (!isEEPROMReady()) {
    clearEEPROM();
    writeEEPROMMagic();
    setU32EEPROM(2, DEFAULT_OPEN_POSITION);
    setU32EEPROM(6, DEFAULT_OPEN_CLOSE_BIT_CHANGE);
    setU32EEPROM(10, DEFAULT_MAX_SPEED);
    setU32EEPROM(14, 0);
  } else {
    open_position = getU32EEPROM(2);
    open_close_bit_change = getU32EEPROM(6);
    motor_max_speed = getU32EEPROM(10);
    close_position = 0;  //getU32EEPROM(14);
  }

  // Vychozi konfigurace GPIO
  pinMode(IR_STOP, INPUT);
  pinMode(IR_GUIDE, INPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  ir_stop = digitalRead(IR_STOP);
  ir_guide = digitalRead(IR_GUIDE);

  // Pripojeni k Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  MDNS.begin("kloboukwi");
  server.on("/", handleRoot);
  server.on("/move", handleMovement);
  server.on("/status", handleStatus);
  server.on("/set", handleSet);
  server.onNotFound(handleNotFound);
  // Priprava pro OTA flash pres HTTP
  server.on(
    "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        WiFiUDP::stopAll();
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) {  // start with max available size
          webSocket.broadcastTXT("UPGRADE ERROR FILE-OVERSIZED");
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          webSocket.broadcastTXT("UPGRADE ERROR FILE-NOT-COMPLETED");
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  // true to set the size to the current progress
          webSocket.broadcastTXT("UPGRADE OK RESTARTING!");
        } else {
          webSocket.broadcastTXT("UPGRADE ERROR FILE-NOT-COMPLETED");
        }
      }
      yield();
    });

  // Konfigurace driveru krokoveho motoru
  // Zatim trosku blackbox, je treba ocistit
  // Chova se nejak divne, bez resetu se sptane nakonfiguruje atp.
  delay(2000);
  dSPIN_init();
  dSPIN_ResetDev();
  delay(1000);
  driverSetup();
  dSPIN_ResetDev();
  delay(1000);
  dSPIN_SetParam(dSPIN_MAX_SPEED, MaxSpdCalc(motor_max_speed));
  pinMode(0, INPUT); // Registrace tlacitka na kabelu
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  pos_check = millis();
  if (digitalRead(IR_STOP) == 1) {
    state = STATE_CLOSED;
    notify();
  }
}

void driverSetup() {
  // First things first: let's check communications. The CONFIG
  //  register should power up to 0x2E88, so we can use that to
  //  check the communications. On the test jig, this causes an
  //  LED to light up.
  //if (dSPIN_GetParam(dSPIN_CONFIG) == 0x2E88)
  // digitalWrite(STAT1, HIGH);

  // The following function calls are for this demo application-
  //  you will need to adjust them for your particular
  //  application, and you may need to configure additional
  //  registers.

  // First, let's set the step mode register:
  //   - dSPIN_SYNC_EN controls whether the BUSY/SYNC pin reflects
  //      the step frequency or the BUSY status of the chip. We
  //      want it to be the BUSY status.
  //   - dSPIN_STEP_SEL_x is the microstepping rate- we'll go full
  //      step.
  //   - dSPIN_SYNC_SEL_x is the ratio of (micro)steps to toggles
  //      on the BUSY/SYNC pin (when that pin is used for SYNC).
  //      Make it 1:1, despite not using that pin.
  dSPIN_SetParam(dSPIN_STEP_MODE,
                 !dSPIN_SYNC_EN | motor_step_resolution | dSPIN_SYNC_SEL_64);

  // Configure the MAX_SPEED register- this is the maximum number
  //  of (micro)steps per second allowed. You'll want to mess
  //  around with your desired application to see how far you can
  //  push it before the motor starts to slip. The ACTUAL
  //  parameter passed to this function is in steps/tick;
  //  MaxSpdCalc() will convert a number of steps/s into an
  //  appropriate value for this function. Note that for any move
  //  or goto type function where no speed is specified, this
  //  value will be used.
  dSPIN_SetParam(dSPIN_MAX_SPEED, MaxSpdCalc(motor_max_speed));

  // Configure the FS_SPD register- this is the speed at which the
  //  driver ceases microstepping and goes to full stepping.
  //  FSCalc() converts a value in steps/s to a value suitable for
  //  this register; to disable full-step switching, you can pass
  //  0x3FF to this register.
  //dSPIN_SetParam(dSPIN_FS_SPD, FSCalc(25));
  dSPIN_SetParam(dSPIN_FS_SPD, 0x3FF);

  // Configure the acceleration rate, in steps/tick/tick. There is
  //  also a DEC register; both of them have a function (AccCalc()
  //  and DecCalc() respectively) that convert from steps/s/s into
  //  the appropriate value for the register. Writing ACC to 0xfff
  //  sets the acceleration and deceleration to 'infinite' (or as
  //  near as the driver can manage). If ACC is set to 0xfff, DEC
  //  is ignored. To get infinite deceleration without infinite
  //  acceleration, only hard stop will work.
  dSPIN_SetParam(dSPIN_ACC, 0xfff);

  // Configure the overcurrent detection threshold. The constants
  //  for this are defined in the L6470.h file.
  dSPIN_SetParam(dSPIN_OCD_TH, motor_overcurrent_detection);

  // Set up the CONFIG register as follows:
  //  PWM frequency divisor = 1
  //  PWM frequency multiplier = 2 (62.5kHz PWM frequency)
  //  Slew rate is 290V/us
  //  Do NOT shut down bridges on overcurrent
  //  Disable motor voltage compensation
  //  Hard stop on switch low
  //  16MHz internal oscillator, nothing on output
  dSPIN_SetParam(dSPIN_CONFIG,
                 dSPIN_CONFIG_PWM_DIV_1 | dSPIN_CONFIG_PWM_MUL_2 | dSPIN_CONFIG_SR_180V_us | dSPIN_CONFIG_OC_SD_DISABLE | dSPIN_CONFIG_VS_COMP_DISABLE | dSPIN_CONFIG_SW_HARD_STOP | dSPIN_CONFIG_INT_16MHZ);

  // Configure the RUN KVAL. This defines the duty cycle of the
  //  PWM of the bridges during running. 0xFF means that they are
  //  essentially NOT PWMed during run; this MAY result in more
  //  power being dissipated than you actually need for the task.
  //  Setting this value too low may result in failure to turn.
  //  There are ACC, DEC, and HOLD KVAL registers as well; you may
  //  need to play with those values to get acceptable performance
  //  for a given application.
  dSPIN_SetParam(dSPIN_KVAL_RUN, 0xFF);

  // Calling GetStatus() clears the UVLO bit in the status
  //  register, which is set by default on power-up. The driver
  //  may not run without that bit cleared by this read operation.
  dSPIN_GetStatus();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) { message += " " + server.argName(i) + ": " + server.arg(i) + "\n"; }
  server.send(404, "text/plain", message);
}

void setSafeState(uint8_t newstate) {
  if (state == STATE_UNKNOWN && digitalRead(IR_STOP) == LOW) {
    state_queue.enqueue(STATE_CLOSE_TO_IR_AND_RESET_POSITION);
    state_queue.enqueue(newstate);
  } else {
    state_queue.enqueue(newstate);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      ws_connected--;
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        // send message to client
        webSocket.sendTXT(num, "KLOBOUKWI WEBSOCKET SERVER");
        webSocket.sendTXT(num, buildDateTime);
        char msg[100];
        sprintf(msg, "RES %d", (int)pow(2, motor_step_resolution));
        webSocket.sendTXT(num, msg);
        sprintf(msg, "OC %d", getOverCurrentDetection(motor_overcurrent_detection));
        webSocket.sendTXT(num, msg);
        ws_connected++;
        notify();
        break;
      }
    case WStype_TEXT:
      {
        //webSocket.sendTXT(num, "ACK");
        if (strcmp((const char*)payload, "open") == 0) {
          setSafeState(STATE_OPEN);
        } else if (strcmp((const char*)payload, "close") == 0) {
          setSafeState(STATE_CLOSE);
        } else if (strcmp((const char*)payload, "status") == 0) {
          notify();
        } else if (strcmp((const char*)payload, "rsthome") == 0) {
          dSPIN_ResetPos();
          notify();
        } else if (payload[0] == 'M' && payload[1] == 'O') {

          setSafeState(STATE_OPEN_BY_STEP);
        } else if (payload[0] == 'M' && payload[1] == 'C') {
          setSafeState(STATE_CLOSE_BY_STEP);
        } else if (payload[0] == 'S' && payload[1] == 'O') {  // SET OPEN POSITION
          open_position = (uint32_t)atoi((char*)payload + 2);
          setU32EEPROM(2, open_position);
          notify();
        } else if (payload[0] == 'R' && payload[1] == 'D') {  // RESET DRIVER
          dSPIN_ResetDev();
          driverSetup();
          notify();
        } else if (payload[0] == 'L' && payload[1] == 'K') {  // LOCK/UNLOCK
          if (lock) {
            dSPIN_Xfer(dSPIN_HARD_HIZ);
            lock = false;
            notify();
          } else {
            dSPIN_Xfer(dSPIN_HARD_STOP);
            notify();
            setSafeState(STATE_CLOSE_TO_IR_AND_RESET_POSITION);
            target = pos;
            setSafeState(STATE_MOVE_TO);
          }
        } else if (payload[0] == 'G' && payload[1] == 'O') {  // BIT STEP SIZE
          target = (uint32_t)atoi((char*)payload + 2);
          setSafeState(STATE_MOVE_TO);
        } else if (payload[0] == 'S' && payload[1] == 'S') {  // BIT STEP SIZE
          open_close_bit_change = (uint32_t)atoi((char*)payload + 2);
          setU32EEPROM(6, open_close_bit_change);
          notify();
        } else if (payload[0] == 'M' && payload[1] == 'M' && payload[2] == 'S') {  // MOTOR MAX SPEED
          motor_max_speed = (uint32_t)atoi((char*)payload + 3);
          setU32EEPROM(10, motor_max_speed);
          dSPIN_SetParam(dSPIN_MAX_SPEED, MaxSpdCalc(motor_max_speed));
          notify();
        } else if (payload[0] == 'R' && payload[1] == 'E') {  // Read EEPROM
          notify_eeprom();
        }
        break;
      }
    case WStype_BIN:
      break;
  }
}

void notify() {
  if (ws_connected > 0) {
    pos = dSPIN_GetParam(dSPIN_ABS_POS);
    char msg[200];
    int irstop = digitalRead(IR_STOP);
    int irguide = digitalRead(IR_GUIDE);
    int btn = digitalRead(BUTTON);
    sprintf(msg, "STATUS %d %d %d %d %d %d %d %d %d %d", state, pos, irstop, irguide, open_position, close_position, open_close_bit_change, motor_max_speed, lock, btn);
    webSocket.broadcastTXT(msg);
  }
}

void notify_eeprom() {
  char msg[100];
  sprintf(msg, "%02X %02X", EEPROM.read(0), EEPROM.read(1));
  webSocket.broadcastTXT(msg);
  for (uint16_t i = 0; i < EEPROM_SIZE / 16; i++) {
    char* ptr = msg;
    for (uint16_t j = 0; j < 16; j++) {
      if (j > 0 && j % 4 == 0) {
        *ptr++ = ' ';
        *ptr++ = '|';
        *ptr++ = ' ';
      }
      ptr += sprintf(ptr, "%02X ", EEPROM.read((i * 16) + 2 + j));
    }
    *(ptr - 1) = '\0';
    webSocket.broadcastTXT(msg);
  }
}

void notify_value(const char* tag, uint32_t value) {
  if (ws_connected > 0) {
    char msg[200];
    sprintf(msg, "%s %d", tag, value);
    webSocket.broadcastTXT(msg);
  }
}

void updateMotorStateMachine() {

  if (!movement) {
    if (!state_queue.isEmpty()) {
      state = state_queue.dequeue();
      notify_value("QUEUED_STATE", state);
      delay(500);
    }
  }

  switch (state) {
    // VYCHOZI STAV
    case STATE_UNKNOWN:
      break;

    // AUTOMATICKE ZAVRENI
    // POVEL CLOSE
    case STATE_CLOSE:
      digitalWrite(LED, LOW);
      dSPIN_GoTo(close_position);
      state = STATE_CLOSING;  // zaviram dvere
      movement = true;
      break;
    // DETEKCE DORAZU A POTE PRECHOD NA STAV 3
    case STATE_CLOSING:
      if (digitalRead(dSPIN_BUSYN) == HIGH || (ir_force_check && digitalRead(IR_STOP) == HIGH)) {  // Probiha zavirani dveri
        dSPIN_SoftStop();
        state = STATE_CLOSED;  // Dvere jsou zavrene
        notify();
        target = close_position;
        delay(500);
        movement = false;
        digitalWrite(LED, HIGH);
      }
      break;

    // MANUALNI ZAVRENI O KOUSEK
    // Start closing by manual step
    case STATE_CLOSE_TO_IR:
      digitalWrite(LED, LOW);
      dSPIN_Move(REV, 999999);
      state = STATE_CLOSING_TO_IR;  // oteviram dvere
      movement = true;
      break;
    // Closing and waiting for IO change
    case STATE_CLOSING_TO_IR:
      if (digitalRead(dSPIN_BUSYN) == HIGH || digitalRead(IR_STOP) == HIGH) {  // Zavirani dokončeno
        dSPIN_SoftStop();
        state = STATE_CLOSED_TO_IR;  // Reset stavu
        notify();
        delay(500);
        movement = false;
        digitalWrite(LED, HIGH);
      }
      break;

    // MANUALNI ZAVRENI O KOUSEK
    // Start closing by manual step
    case STATE_CLOSE_TO_IR_AND_RESET_POSITION:
      digitalWrite(LED, LOW);
      dSPIN_Move(REV, 999999);
      state = STATE_CLOSING_TO_IR_AND_RESET_POSITION;  // oteviram dvere
      movement = true;
      break;
    // Closing and waiting for IO change
    case STATE_CLOSING_TO_IR_AND_RESET_POSITION:
      if (digitalRead(dSPIN_BUSYN) == HIGH || digitalRead(IR_STOP) == HIGH) {  // Zavirani dokončeno
        dSPIN_SoftStop();
        state = STATE_CLOSED_TO_IR_AND_RESETED_POSITION;  // Reset stavu
        dSPIN_ResetPos();
        notify();
        delay(500);
        movement = false;
        digitalWrite(LED, HIGH);
      }
      break;

    // AUTOMATICKE OTEVRENI
    // POVEL OPEN
    case STATE_OPEN:
      digitalWrite(LED, LOW);
      dSPIN_GoTo(open_position);
      state = STATE_OPENING;  // oteviram dvere
      movement = true;
      break;
    // OPENNING
    case STATE_OPENING:
      if (digitalRead(dSPIN_BUSYN) == HIGH) {  // Otevirani dokončeno
        dSPIN_SoftStop();
        state = STATE_OPENED;  // Reset stavu
        notify();
        target = open_position;
        delay(500);
        movement = false;
        digitalWrite(LED, HIGH);
      }
      break;


    case STATE_MOVE_TO:
      digitalWrite(LED, LOW);
      dSPIN_GoTo(target);
      state = STATE_MOVING_TO;  // oteviram dvere
      movement = true;
      break;
    // OPENNING
    case STATE_MOVING_TO:
      if (digitalRead(dSPIN_BUSYN) == HIGH) {  // Otevirani dokončeno
        dSPIN_SoftStop();
        state = STATE_MOVED_TO;  // Reset stavu
        notify();
        delay(500);
        movement = false;
        digitalWrite(LED, HIGH);
      }
      break;

    // MANUALNI OTEVRENI O KOUSEK
    // Start opening by manual step
    case STATE_OPEN_BY_STEP:
      digitalWrite(LED, LOW);
      dSPIN_Move(FWD, open_close_bit_change);
      state = STATE_OPENING_BY_STEP;  // oteviram dvere
      movement = true;
      break;
    // Opening and waiting for IO change
    case STATE_OPENING_BY_STEP:
      if (digitalRead(dSPIN_BUSYN) == HIGH) {  // Otevirani dokončeno
        dSPIN_SoftStop();
        state = STATE_OPENED_BY_STEP;  // Reset stavu
        notify();
        target = dSPIN_GetParam(dSPIN_ABS_POS);
        delay(500);
        movement = false;
        digitalWrite(LED, HIGH);
      }
      break;

    // MANUALNI ZAVRENI O KOUSEK
    // Start closing by manual step
    case STATE_CLOSE_BY_STEP:
      digitalWrite(LED, LOW);
      dSPIN_Move(REV, open_close_bit_change);
      state = STATE_CLOSING_BY_STEP;  // oteviram dvere
      movement = true;
      break;
    // Closing and waiting for IO change
    case STATE_CLOSING_BY_STEP:
      if (digitalRead(dSPIN_BUSYN) == HIGH) {  // Zavirani dokončeno
        dSPIN_SoftStop();
        state = STATE_CLOSED_BY_STEP;  // Reset stavu
        notify();
        target = dSPIN_GetParam(dSPIN_ABS_POS);
        delay(500);
        movement = false;
        digitalWrite(LED, HIGH);
      }
      break;
  }
}

void check_ir_guide_change() {
  if (ir_guide != digitalRead(IR_GUIDE)) {
    ir_guide = digitalRead(IR_GUIDE);
    notify();
  }
}

void check_ir_stop_change() {
  if (ir_stop != digitalRead(IR_STOP)) {
    ir_stop = digitalRead(IR_STOP);
    notify();
  }
}

void check_guide_ready() {
  static uint8_t oldstate = STATE_UNKNOWN;
  static bool oldmovement = false;
  if (digitalRead(IR_GUIDE) == LOW) {
    if (state != STATE_NO_GUIDE) {
      dSPIN_SoftStop();
      oldstate = state;
      state = STATE_NO_GUIDE;
      oldmovement = movement;
      movement = false;
    }
  } else {
    if (state == STATE_NO_GUIDE) {
      movement = oldmovement;
      state = oldstate;
    }
  }
}

void check_notify_scheduler() {
  if ((millis() - pos_check > 100) && movement) {
    pos_check = millis();
    notify();
  }
}

void check_button() {
  static uint8_t lastButtonState = 1;
  static uint8_t buttonState = 1;
  static uint32_t lastDebounceTime = 0;
  static uint32_t buttonPressTime = 0;
  const uint32_t debounceDelay = 50;
  const uint32_t longPressDuration = 2000;
  uint8_t reading = digitalRead(BUTTON);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == 0) {
        buttonPressTime = millis();
        // Stisk tlacitka
        notify_value("BUTTON", buttonState);
      } else {
        notify_value("BUTTON", buttonState);
        // Uvolneni tlacitka po stisku
        if ((millis() - buttonPressTime) >= longPressDuration) {
          // Pri dlouhem stisku prepneme zamek
          if (lock) {
            dSPIN_Xfer(dSPIN_HARD_HIZ);
            lock = false;
            notify();
          } else {
            dSPIN_Xfer(dSPIN_HARD_STOP);
            notify();
            setSafeState(STATE_CLOSE_TO_IR_AND_RESET_POSITION);
            target = pos;
            setSafeState(STATE_MOVE_TO);
          }
        } else {
          // Pri kratkem stisku iniciujeme bud otevreni, nebo zavreni
          if (state == STATE_CLOSED) {
            state = STATE_OPEN;
          } else {
            state = STATE_CLOSE;
          }
        }
      }
    }
  }
  lastButtonState = reading;
}

void loop() {
  check_guide_ready();
  check_ir_guide_change();
  check_ir_stop_change();
  check_button();

  updateMotorStateMachine();

  check_notify_scheduler();

  server.handleClient();
  MDNS.update();
  webSocket.loop();
  delay(1);
}
