/*
  Dnes ozivime prototypovaci kuchynskou vahu
  Open - Smart Weight Sensor z AliExpressu
  https://www.aliexpress.com/item/4001242085837.html

  Vse ozivime pomoci desky Arduino Uno a udaj o hmotnosti
  vypisime na libovolny ctyrcislicovy segmentovy dsiplej s radicem TM1637
*/

// Knihovna pro praci s A/D prevodnikem HX711
// www.arduino.cc/reference/en/libraries/gyverhx711/
#include <GyverHX711.h>
// Knihovna pro praci se segmentovym displejem s radicem TM1637
// www.arduino.cc/reference/en/libraries/gyvertm1637/
#include <GyverTM1637.h>

// Objekt vahy
// Pin SCL je pripojeny na pin 8 desky Arduino Uno
// Pin DAT je pripojeny na pin 9 desky Arduino Uno
// Vahu napajime z pinu 3,3V a GND
GyverHX711 vaha(8, 9);

// Objekt displeje
// Pin CLK je pripojeny na pin 7 desky Arduino Uno
// Pin DIO je pripojeny na pin 6 desky Arduino Uno
// Displej napajime z pinu 5V a GND
GyverTM1637 displej(7, 6);

long nula = 0; // Kalibracni hodnota predstavujici 0 kg
long kilogram = 0; // Kalibracni hodnota predstavujici 1 kg
long petkilogramu = 0; // // Kalibracni hodnota predstavujici 5 kg

// Funkce pro ziskani zprumerovane/zhlazene hodnoty
// V parametru lze nastavit pocet hodnot pro vypocet aritmetickeho prumeru
long ziskejPrumer(int opakovani) {
  uint32_t suma = 0;
  int pozice = 0;
  long adc = 0;
  // Nez zacneme ukladat hodnoty z A/D,
  // prvni mereni zahodime. Muze obsahovat stare hodnoty
  if (vaha.available()) {
    adc = vaha.read();
  }
  // Cteme tak dlouho, dokud neziskame pozadovany pocet hodnot
  // Pozor, pokud spojeni s A/D prevodnikem nebude fungovat,
  // tato smycka bude blokovat beh az do konce veku
  while (pozice < opakovani) {
    if (vaha.available()) {
      adc = vaha.read();
      if (adc > 0) {
        suma += adc;
        pozice++;
      }
    }
  }
  long prumer = suma / opakovani;
  return prumer;
}

// Pomocna funkce, ktera blokuje dalsi beh programu,
// dokud mikrokontroler nedostane od seriove linky pozadovany znak
void cekejNaZnak(char hledej) {
  char znak;
  while (znak != hledej) {
    while (Serial.available()) {
      znak = (char)Serial.read();

    }
  }
}

// Hlavni funkce setup nastartuje jako prvni
void setup() {
  Serial.begin(115200); // Start seriove linky do PC rychlosti 115 200 b/s
  displej.clear(); //  Vymazani displeje
  displej.brightness(7); // Nastaveni jasu na maximum

  // Kalibrace klidove hodnoty
  // Vaha bez zateze
  Serial.println("Kalibrace: Odstran z vahy predmety a odesli znak '0'");
  cekejNaZnak('0');
  Serial.print("Kalibruji... ");
  nula = ziskejPrumer(20);
  Serial.println("OK");

  // Kalibrace 1 kg
  Serial.println("Kalibrace: Poloz na vahu 1 kg a odesli znak '1'");
  cekejNaZnak('1');
  Serial.print("Kalibruji... ");
  kilogram = ziskejPrumer(20) - nula;
  petkilogramu = nula + (kilogram * 5);
  Serial.println("OK");

  // Kontrolni vypis zjistenych udaju
  // Mohli bychom je ulozit do EEPROM nebo jine persistentni pameti
  Serial.println("Skvele, vaha zkalibrovana!");
  Serial.print("0 kg = "); Serial.println(nula);
  Serial.print("1 kg = "); Serial.println(kilogram);
  Serial.print("5 kg = "); Serial.println(petkilogramu);
}

// Funkce loop se spusti po zpracovani funkce setup
// A jeji obsah se opakuje stale dokola
void loop() {
  // Pokud mame data od A/D prevodniku HX711
  if (vaha.available()) {
    // Precti hodnotu
    long adc = vaha.read();
    // Orez hodnotu podle kalibracniho minima a maxima
    adc = constrain(adc, nula, petkilogramu);
    // Podle kalibracniho minima a maxima ji prepocitej na rozsah 0-5000 gramu
    long hmotnost_g = map(adc, nula, petkilogramu, 0, 5000);

    // Vypis hodnotu v gramech do seriove linky a na segmentovy displej
    Serial.println(hmotnost_g);
    displej.displayInt(hmotnost_g);
  }
}
