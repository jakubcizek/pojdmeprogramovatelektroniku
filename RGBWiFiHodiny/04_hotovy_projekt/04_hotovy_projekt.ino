/*
 * Hodiny s prstencem a internetovym casem
 *
 * Pouzite knihovny:
 * Adafuit NeoPixel: https://github.com/adafruit/Adafruit_NeoPixel
 * TimeLib: https://github.com/PaulStoffregen/Time
 * TM1637: https://github.com/Seeed-Studio/Grove_4Digital_Display
 *
 * Pouzita deska:
 * Wemos D1 Mini a kompatibilni klon
 * nebo NodeMCU v3 (nevejde se do krabicky)
 * 
 * Kryt pro 3D tisk:
 * www.tinkercad.com/things/9lU8NQrdpXy
*/

#include <ESP8266WiFi.h> // Wi-Fi na ESP8266
#include <WiFiUdp.h> // UDP na ESP8266

#include <Adafruit_NeoPixel.h> // Prstenec
#include <TM1637.h> // Segmentovy displej
#include <TimeLib.h> // Internetovy cas

char ssid[] = "123456789"; // SSID Wi-Fi site
char heslo[] = "123456789"; // Heslo Wi-Fi site

char serverNTP[] = "pool.ntp.org"; // NTP server, seznam: https://www.ntppool.org/
int casovaZona = 2; // Casova zona, mame letni cas, takze GMT+2

Adafruit_NeoPixel prstenec(24, D5, NEO_GRB + NEO_KHZ800); // Prstenec je pripojeny na GPIO D5
TM1637 displej(D1, D2); // Segmentovy displej je pripojeny na GPIO D1 (CLK) a D2 (DIO)

// Pole s RGB barvami 24 jednotek LED prstence
uint8_t pixely[24][3] = {
  {0, 50, 0},
  {0, 50, 0},
  {0, 50, 0},
  {0, 50, 0},
  {0, 50, 0},
  {0, 50, 0},
  {0, 0, 50},
  {0, 0, 50},
  {0, 0, 50},
  {0, 0, 50},
  {0, 0, 50},
  {0, 0, 50},
  {50, 0, 0},
  {50, 0, 0},
  {50, 0, 0},
  {50, 0, 0},
  {50, 0, 0},
  {50, 0, 0},
  {50, 50, 50},
  {50, 50, 50},
  {50, 50, 50},
  {50, 50, 50},
  {50, 50, 50},
  {50, 50, 50}
};

WiFiUDP Udp; // S NTP serverem budeme komunikovat skrze rychly protokol UDP
uint16_t udpPort = 8888; // UDP port
uint8_t ntpPaket[48]; // 48B na zpravu pro NTP server a odpoved

uint8_t dvojtecka = 1; // Promenna ridici blikani dvojtecky segmentoveho displeje
time_t staryCas = 0; // Pomocna promenna pro porovnani, jeslti uz se zmenil cas ve smycce loop

// Hlavni funkce setup se spousti hned po startu cipu
void setup() {

  prstenec.begin(); // Priprav prstenec
  prstenec.setBrightness(20); // Relativne nizky jas, aby krabicka nezarila prilis
  prstenec.clear(); // Vypni vsechny LED
  prstenec.show();

  displej.init(); // Nastartuj displej
  displej.set(7); // Nejvyssi jas (0-7)
  displej.point(dvojtecka); // Vykreslit dvojtecku mezi hodinami a minutami

  Serial.begin(115200); // Nastartuj seriovou linku do PC
  delay(100); // Chvili pockej
  Serial.println(); // Odradkuj pro prehlednost

  WiFi.persistent(false); // Reset Wi-Fi
  WiFi.mode(WIFI_OFF); // Reset Wi-Fi
  WiFi.mode(WIFI_STA); // Nastav Wi-Fi jako stanici
  WiFi.begin(ssid, heslo); // Zacni se pripojovat k Wi-Fi

  // Dokud nebudes pripojey k Wi-Fi vypisuj tecky
  Serial.print("Pripojuji k " + String(ssid) + " ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  // Po pripojeni k Wi-Fi vypis DHCP IP adresu
  Serial.println(" OK");
  Serial.print("Moje LAN IP adresa: ");
  Serial.println(WiFi.localIP());

  Udp.begin(udpPort); // Nastartuj UDP
  setSyncProvider(ziskejInternetovyCas); // Casova knihovna bude pouzivat k synchonizaci tuto funkci
  setSyncInterval(300); // Casova knihovna bude synchronizovat cas kazdych 300 sekund
}

// Nekonecna smycka loop se spusti po zpracovani funcke setup a jeji obsah se opakuje stale dokola
void loop() {
  if (timeStatus() != timeNotSet) { // Zna uz casova knihovna cas?
    /* Smycku loop nechceme zbytecne blokovat prikazem delay,
     * a tak porovname aktualni cas s tim starym ulozenym v pomocne knihovne,
     * a pokud se uz zmenil (pribyla sekunda), tprve tehdy
     * vypiseme zpravu do seriove linky s aktualnim casem
    */
    if (now() != staryCas) {
      staryCas = now();

      displej.point(dvojtecka ^= 1); // Prohod nastaveni dvojtecky v segmentovem displeji, takze bude blikat

      char txtHodiny[5];
      sprintf(txtHodiny, "%02d%02d", hour(), minute());
      displej.displayStr(txtHodiny, 0);

      prstenec.clear();
      for (uint8_t i = 0; i < map(second(), 0, 59, 0, 24); i++)
        prstenec.setPixelColor(i, prstenec.Color(pixely[i][0], pixely[i][1], pixely[i][2]));
      prstenec.show();
    }
  }
}

/* Funkce pro ziskani aktualniho casu z NTP serveru pomoci UDP protokolu
 * Tuto funkci si bude volat automaticky casova knihovna behem prubezne synchronizace
*/
time_t ziskejInternetovyCas() {
  while (Udp.parsePacket() > 0) ; // Pokud z UDP prichazeji nejaka data, pockej
  IPAddress ntpServerIP;
  WiFi.hostByName(serverNTP, ntpServerIP); // Preved domenu casoveho serveru na IP adresu
  posliPaketNTP(ntpServerIP); // Odesli UDP davku na casovy server
  uint32_t cekej = millis();
  while (millis() - cekej < 1500) { // Cekej nejdele 1,5 sekudny na odpoved
    int size = Udp.parsePacket(); // Precti veliksot odpovedi
    if (size >= 48) { // Odpoved musi mit nejvyse 48 bajtu
      Udp.read(ntpPaket, 48); // Precti odpoved do pole ntpPacket
      /* 40-43 bajt odpovedi prepocitej na uplynule sekundy od roku 1900
       * a pripocitej take mistni casovou zonu (bezny cas: +1, letni cas +2)
      */
      uint32_t sekundyOd1900;
      sekundyOd1900 =  (uint32_t)ntpPaket[40] << 24;
      sekundyOd1900 |= (uint32_t)ntpPaket[41] << 16;
      sekundyOd1900 |= (uint32_t)ntpPaket[42] << 8;
      sekundyOd1900 |= (uint32_t)ntpPaket[43];
      return sekundyOd1900 - 2208988800UL + casovaZona * SECS_PER_HOUR;
    }
  }
  return 0; // Pokud opdoved nedorazila do 1,5 sekundy, vrat nulu (sync se nepovedl)
}

/* Funkce pro odeslani paketu na NTP server
 * Jednotlive bajty obsahuji instrukci, aby server vedel, co po nem chceme
 * Jakmile paket odesleme, ve funkci ziskejInternetovyCas budeme cekat, dokud
 * nedorazi paket s odpovedi
*/
void posliPaketNTP(IPAddress &ip) {
  memset(ntpPaket, 0, 48);
  ntpPaket[0] = 0b11100011;
  ntpPaket[1] = 0;
  ntpPaket[2] = 6;
  ntpPaket[3] = 0xEC;
  ntpPaket[12] = 49;
  ntpPaket[13] = 0x4E;
  ntpPaket[14] = 49;
  ntpPaket[15] = 52;
  Udp.beginPacket(ip, 123); // Spoj se s NTP serverem na UDP portu 123
  Udp.write(ntpPaket, 48); // Posli mu paket o 48 B, ktery jsme si prave vytvorili
  Udp.endPacket(); // Ukonci paket
}
