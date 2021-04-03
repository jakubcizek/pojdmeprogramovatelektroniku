/*
 * Demoukazka segmentoveho displeje s radicem TM1647
 * Pouzite knihovny:
 * TM1637: https://github.com/Seeed-Studio/Grove_4Digital_Display
 * Pouzita deska: NodeMCU v3 s cipem ESP8266
*/

#include <TM1637.h>

/*
 * Parametry tridy displej
 * CLK signal: pin D1
 * DIO signal: pin D2
*/
TM1637 displej(D1, D2);

void setup() {
  displej.init(); // Nastartuj displej
  displej.set(4); // Stredni jas (0-7)
  displej.point(1); // Vykreslit dvojtecku mezi hodinami a minutami
  uint8_t hodina = 8; // Hodina
  uint8_t minuta = 3; // Minuta
  char txtHodiny[5]; // Textova promenna pro cas
  sprintf(txtHodiny, "%02d%02d", hodina, minuta); // Vytvor ctyri znaky ve formatu HHMM, tedy 0803
  /*
   * Posli textova data na displej
   * Kdybychom chteli poslat vice nez 4 znaky, text zacne po znacich rotovat
   * Rychlost rotace v milisekundach urcuje posledni parametr
  */
  displej.displayStr(txtHodiny, 0);
}

void loop() {}
