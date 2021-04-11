/*
 * 2D LiDAR, cast pro Arduino, ktera meri vzdalenost a otaci motorem
 *
 * Pouzite knihovny:
 * Servo: https://www.arduino.cc/reference/en/libraries/servo/attach/
 * TFLuna-I2C: https://github.com/budryerson/TFLuna-I2C
 *
 * Pouzita deska:
 * Arduino Uno (pozor, kvuli 5V logice pouzijeme prevodnik logickych signalu, TF-Luna pracuje na 3V!)
*/

#include <Servo.h> // Ovladani serva skrze PWM
#include <Wire.h> // Prace se sbernici I2C
#include <TFLI2C.h> // Spojeni s dalkoemrem TF-Luna

// Objekt servomotoru
Servo servo;

// Objekt laseroveho dalkomeru
TFLI2C dalkomer;

// Promenna se zmerenou vzdalenosti
int16_t vzdalenost;

// Hlavni funkce setup se zpracuje hned po startu,
// jakmile pripojime mikrokontroler ke zdroji napajeni
void setup() {
  Serial.begin(115200); // Nastartuj seivou linku na rychlost 115 200 b/s
  Wire.begin(); // Nastartuj sbernici I2C
  /*
   * Nastartuj servo, jehoz ovladaci pin je pripojeny na GPIO cislo 3 (pouzivame desku Arduino Uno)
   * Dalsi dva parametry urcuji sirku pulzu v mikrosekundach pro nejmensi a nejvetsi uhel nakloneni
   * Je treba nahledno ut do kdokuemtnace serva, anebo tyto hodnoty nepouzivat a nechat ty vychozi
  */
  servo.attach(3, 544, 2400);

  // Kontrolni natoceni na oba dorazy
  servo.write(180); // Nastav servo na uhel 180
  delay(5000); // Pockej 5 sekund
  servo.write(0); // Nastav servo na uhel 0
  delay(5000); // Pockame 5 sekund
}

// Nekonecna smycka loop se spusti po zpracovani funkce setup
void loop() {
  // Projdi hodnoty 0 az 180 vcetne
  for (int16_t pozice = 0; pozice <= 180; pozice++) {
    servo.write(pozice); // Nastav motor na dany uhel
    delayMicroseconds(500); // Pockej 500 us, aby se stihl motor natocit (s casem lze experimentovat podle rychlosti serva)
    // Pokusime z dalkomeru precist vzdalenost
    // Dalkomer ma vyrobni I2C adresu 0x10
    if (dalkomer.getData(vzdalenost, 0x10)) {
      // Pokud se mereni podari, vypis do seriove linky radek vcetne zalomeni UHEL VZDALENOST
      Serial.print(pozice);
      Serial.print(" ");
      Serial.println(vzdalenost);
    }
  }

  /*
   * Zmerili jsme cely rozsah, a tak se vratime do vychozi pozice
   * Nase lacine servo neni prilis presne, a proto po ceste zpet nemerime
   * Merime tedy vzdy jen v jednom smeru, aby nam kroky co nejvice odpovidaly skutecnemu naklonu
   * Neni to efektivni, ale v ramci moznosti relativne nejpresnejsi
  */
  servo.write(0);
  delay(250);

  /*
   * Konec pruchodu smyckou loop
   * Servo se otaci z jednoho 180stupnoveho dorazu k druhemu,
   * po kazdem stupnovem skoku zmeri dalkomer vzdalenost
   * a posle udaj skrze seriovou linku do PC,
   * kde druhy program v Processingu kresli graf
  */
}
