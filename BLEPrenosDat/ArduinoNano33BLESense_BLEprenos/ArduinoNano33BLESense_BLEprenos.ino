#include <ArduinoBLE.h> // Pouzijeme knihovnu pro Bluetooth LE komunikaci
#include <Arduino_HTS221.h> // Pouzijeme knihovnu pro cip teplomeru HTS221

// Vytvorime novou BLE sluzbu s vlastnim UUID
// s vygnerovanim unikatniho UUID pomuze treba web https://www.guidgenerator.com/
BLEService teplomerBLESluzba("0E633B12-4B56-47EB-915A-3015E74E37BD");

// Sluzba vyse bude mit dve osmibitové charakteristiky pro teplotu a vlhkost
// Kazda charakteristika ma take unikatni UUID a vlastnost BLERead (budeme z ni cist) a BLENotify (bude se automaticky aktualizovat)
BLEUnsignedCharCharacteristic teplotaBLECharakteristika("0E633B13-4B56-47EB-915A-3015E74E37BD", BLERead | BLENotify);
BLEUnsignedCharCharacteristic vlhkostBLECharakteristika("0E633B14-4B56-47EB-915A-3015E74E37BD", BLERead | BLENotify);

// Pomocna promenna pro kontrolu periodickeho mereni
uint32_t posledniMereni = 0;

// Hlavni funkce setup se zpracuje hned na zacatku
void setup() {
  Serial.begin(115200); // Nastartujeme seriovou linku na USB
  while (!Serial); // Pockame, dokud v PC neotevreme seriovy monitor
  HTS.begin(); // Nastartujeme teplomer
  BLE.begin(); // Nastartujeme Bluetooth LE
  BLE.setLocalName("Teplomer"); // Nastavime nazev BLE zarizeni
  BLE.setAdvertisedService(teplomerBLESluzba); // Nastavime sluzbu, kterou budeme vysilat v ramci BLE advertisignu
  teplomerBLESluzba.addCharacteristic(teplotaBLECharakteristika); // Pripojime ke sluzbe charakteristiku teploty
  teplomerBLESluzba.addCharacteristic(vlhkostBLECharakteristika); // Pripojime ke sluzbe charakteristiku vlhkosti
  BLE.addService(teplomerBLESluzba); // Pridame sluzbu do BLE
  teplotaBLECharakteristika.writeValue((uint8_t)HTS.readTemperature()); // Nastavime vychozi hodnotu charakteristiky teplota na aktualni teplotu
  vlhkostBLECharakteristika.writeValue((uint8_t)HTS.readHumidity()); // Nastavime vychozi hodnotu charakteristiky vlhkost na aktualni vlhkost
  BLE.advertise(); // Konecne vyvolame BLE advertising -- BLE vysilac vysila do okoli, jak se jmenuje a co umi (UUID sluzby)
  Serial.println("Bluetooth LE aktivni, cekam na spojeni...");
}

// Nekonecna smycka loop se spusti po zpracovani funkce setup
void loop() {
  BLEDevice klient = BLE.central(); // Pripojuje se k nam nejaky klient?

  if (klient) { // Pokud ano, vypiseme do seriove linky jeho BLE adresu
    Serial.print("Pripojeno k: ");
    Serial.println(klient.address());
    while (klient.connected()) { // Je uz klient pripojeny?
      if (abs(millis() - posledniMereni) >= 2000) { // Pomoci pomocne promenne a poctu milisekund od statu porovname, jestli uz ubehly dve sekundy (interval mereni)
        uint8_t teplota = (uint8_t)HTS.readTemperature(); // Zmer teplotu a uloz z ni jen celociselny kladny udaj bez desetinne carky
        uint8_t vlhkost = (uint8_t)HTS.readHumidity(); // Stejnym zpusobem uloz vlhkost
        Serial.print(teplota); Serial.println(" °C"); // Vypis teplotu pro kontrolu do seriove linky
        Serial.print(vlhkost); Serial.println(" %"); // Vypis stejnym zpusobem vlhkost
        teplotaBLECharakteristika.writeValue(teplota); // Zapis teplotu do jeji BLE charakteristiky (teprve ted dochazi k radiovemu prenosu do pripojeneho zarizeni)
        vlhkostBLECharakteristika.writeValue(vlhkost); // Zapis vlhkost do jeji BLE charakteristiky
        posledniMereni = millis(); // Nastav pomocnou promennou na aktualni pocet milisekund od startu
      }
    }
    // Pokud uz neni klient pripojeny, pro kontrolu napis, ze se prave odpojil
    Serial.print("Klient ");
    Serial.print(klient.address());
    Serial.println(" se odpojil");
  }
} // Konec funkce loop, smycka se opakuje zase od zacatku
