#include <Arduino.h>
#include <BLEDevice.h> // Knihovna pro práci s BLE, součást ESP32 SDK pro Arduino
#include <WiFi.h> // Knihovna pro práci s Wi-Fi, součást ESP32 SDK pro Arduino
#include <HTTPClient.h> // Knihovna HTTP klientu, součást ESP32 SDK pro Arduino
#include <ArduinoJson.h> // Knihovna pro práci s JSON, https://arduinojson.org
#include <TimeLib.h> // Knihovna pro práci s časem, https://github.com/PaulStoffregen/Time

#define MAX_THERMOMETERS 20 // Rezervace paměti pro maximální počet teploměrů
#define HTTP_REFRESH_MS 300000UL // Nová data posíláme na web každých 5 minut
#define URL "http://rpizero2.local/teplomery" // URL pro odesílání dat
#define URLTIMESYNC "http://rpizero2.local/unixtime" // URL pro synchronizaci času

const char* ssid = ""; // Wi-Fi SSID
const char* pass = ""; // Wi-Fi heslo

BLEScan* blescan; // Ukazatel na BLE skener

// Struktura našeho teploměru
struct ThermometerData {
  String name;
  String mac;
  int16_t rssi;
  float temperature;
  uint8_t humidity;
  uint8_t battery_percent;
  uint16_t battery_mv;
  uint8_t counter;
  time_t dt;
};

ThermometerData dataCache[MAX_THERMOMETERS]; // Pole pro ukládání dat teploměrů
int numThermometers = 0; // Počet aktuálně nalezených teploměrů
uint32_t lastPostTime = 0; // Čas posledního odeslání dat

// Aktualizace dat pro teploměr – pokud již existují, přepíšeme je
void updateThermometerData(const ThermometerData &newData) {
  for (int i = 0; i < numThermometers; i++) {
    if (dataCache[i].mac == newData.mac) {
      dataCache[i] = newData;
      return;
    }
  }
  if (numThermometers < MAX_THERMOMETERS) {
    dataCache[numThermometers++] = newData;
  }
}

// Callback pro zpracování nalezených BLE zařízení v rádiovém dosahu
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    String name = advertisedDevice.getName().c_str();
    if (advertisedDevice.haveServiceData() && name.startsWith("ATC_")) {
      String mac = advertisedDevice.getAddress().toString().c_str();
      std::string serviceData = advertisedDevice.getServiceData();

      Serial.print(name);
      Serial.print(" | MAC: ");
      Serial.print(mac);
      Serial.print(" | RSSI: ");
      Serial.print(advertisedDevice.getRSSI());
      Serial.print(" | Data: ");
      for (size_t i = 0; i < serviceData.length(); i++) {
        Serial.printf("%02X ", (uint8_t)serviceData[i]);
      }

      if (serviceData.length() == 13) {
        const uint8_t* data = (const uint8_t*)serviceData.data();
        int16_t temp_raw = (data[6] << 8) | data[7];
        float temperature = temp_raw / 10.0;
        uint8_t humidity = data[8];
        uint8_t battery_percent = data[9];
        uint16_t battery_mv = (data[10] << 8) | data[11];
        uint8_t counter = data[12];

        Serial.printf(" | Teplota: %.2f °C | Vlhkost: %u %% | Baterie: %u %% (%u mV) | Pocitadlo: %u\r\n",
          temperature, humidity, battery_percent, battery_mv, counter);

        ThermometerData newData;
        newData.name = name;
        newData.mac = mac;
        newData.rssi = advertisedDevice.getRSSI();
        newData.temperature = temperature;
        newData.humidity = humidity;
        newData.battery_percent = battery_percent;
        newData.battery_mv = battery_mv;
        newData.counter = counter;
        newData.dt = now();
        updateThermometerData(newData);
      } else {
        Serial.printf(" | Service Data má neočekávanou délku (%d B)\r\n", serviceData.length());
      }
    }
  }
};


// Funkce pro nastavení času pomocí HTTP GET
uint32_t getWiFiTime(){
  uint32_t unixtime = 0;
  WiFi.begin(ssid, pass);
  Serial.print("Připojuji se k WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK");
  HTTPClient http;
  http.begin(URLTIMESYNC);
  int httpResponseCode = http.GET();
  if(httpResponseCode > 0) {
    String payload = http.getString();
    JsonDocument httpdoc;
    DeserializationError error = deserializeJson(httpdoc, payload);
    if (!error) {
      unixtime = httpdoc["unixtime"];
      Serial.print("Čas byl nastaven na: ");
      Serial.println(unixtime);
    } else {
      Serial.print("Chyba při parsování JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("HTTP GET selhalo: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  WiFi.disconnect();
  return unixtime;
}

// Hlavní funcke setup
void setup() {
  Serial.begin(115200);

  // Nastavení času pomocí HTTP GET
  setTime(getWiFiTime()); 

  // Inicializace BLE skenování
  BLEDevice::init("");
  blescan = BLEDevice::getScan();
  blescan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  blescan->setActiveScan(true);
  blescan->setInterval(100);
  blescan->setWindow(99);
  lastPostTime = millis();
}

// Smyčka loop pro periodicke BLE skenování a odesílání dat skrze Wi-Fi
void loop() {
  // Spustíme krátké BLE skenování
  blescan->start(1, false);
  blescan->clearResults();
  delay(100);
 
  // Pokud mame data z alespoň jednoho teploměru a uplynulo HTTP_REFRESH_MS od posledního odeslání
  if ((numThermometers > 0) && (millis() - lastPostTime >= HTTP_REFRESH_MS)) {
    lastPostTime = millis();
      
    // Obvod 2,4GHz rádia může pracovat jen v jednom režimu
    // Vypneme tedy BLE skenování, abychom uvolnili prostředky pro Wi-Fi
    blescan->stop();

    // Připojíme se k Wi-Fi
    WiFi.begin(ssid, pass);
    Serial.print("Připojuji se k WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println(" OK");

    // Vytvoříme JSON z pole teploměrů
    // Na ESP32 bychom měli mít v dynamické paměti dostatek místa
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    for (int i = 0; i < numThermometers; i++) {
      JsonObject obj = array.add<JsonObject>();
      obj["name"] = dataCache[i].name;
      obj["mac"] = dataCache[i].mac;
      obj["rssi"] = dataCache[i].rssi;
      obj["temperature"] = dataCache[i].temperature;
      obj["humidity"] = dataCache[i].humidity;
      obj["battery_percent"] = dataCache[i].battery_percent;
      obj["battery_mv"] = dataCache[i].battery_mv;
      obj["counter"] = dataCache[i].counter;
      obj["unixtime"] = (unsigned long) dataCache[i].dt;
    }
    
    // A teď celý JSON převedeme na String, takže další alokace!
    // Při odesílání bychom tedy mohli JSON rovnou nahradit ruční tvorbou ve Stringu, respektive char*
    // Brána ale nic dalšího a paměťově náročného nedělá, takže i v tomto případě by to mělo být v pořádku
    String json;
    serializeJson(doc, json);
    Serial.println("Odesílám JSON:");
    Serial.println(json);

    // Odeslání HTTP POST požadavku
    HTTPClient http;
    http.begin(URL);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(json);
    Serial.print("HTTP odpověď: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println("Odpověď serveru:"); 
    Serial.println(payload);
    JsonDocument httpdoc;
    DeserializationError error = deserializeJson(httpdoc, payload);
    if (!error) {
      // Kontrola, zda server vrátil true
      bool ret = httpdoc["return"];
      if (ret) {
        // Načtení unixtime z JSON a nastavení času
        uint32_t unixtime = httpdoc["unixtime"];
        setTime(unixtime);
        Serial.print("Čas byl nastaven na: ");
        Serial.println(unixtime);
      } else {
        Serial.println("Server vrátil false, čas nebyl nastaven");
      }
    } else {
      Serial.print("Chyba při parsování JSON: ");
      Serial.println(error.c_str());
    }
    
    http.end();

    // Odpojení WiFi
    WiFi.disconnect();

    // Vyčistíme cache a resetujeme časovač
    numThermometers = 0;

    // Po odeslání opět spustíme BLE skenování
    Serial.println("Opět spouštím BLE skenování...");
    blescan->start(1, false);
  }
}
