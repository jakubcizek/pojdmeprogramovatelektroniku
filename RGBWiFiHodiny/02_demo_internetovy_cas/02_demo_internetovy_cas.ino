/*
 * Demoukazka synchronizace NTP casu skrze Wi-Fi na cipu ESP8266
 * Pouzite knihovny:
 * TimeLib: https://github.com/PaulStoffregen/Time
 * Pouzita deska: NodeMCU v3 nebo jina s cipem ESP8266
*/

#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

char ssid[] = "123456789"; // SSID Wi-Fi site
char heslo[] = "123456789"; // Heslo Wi-Fi site

char serverNTP[] = "0.cz.pool.ntp.org"; // NTP server, seznam: https://www.ntppool.org/
uint8_t casovaZona = 2; // Letni cas, GMT+2

WiFiUDP Udp; // S NTP serverem budeme komunikovat skrze rychly protokol UDP
uint16_t udpPort = 8888; // UDP port
byte ntpPaket[48]; // 48B na zpravu pro NTP server a odpoved

time_t staryCas = 0; // Pomocna promenna pro smycku loop, jestli se uz zmenil cas

// Hlavni funkce setup se spousti po startu cipu
void setup() {
  Serial.begin(115200); // Nastartuj seriovou linku do PC
  delay(100);
  Serial.println();

  WiFi.persistent(false); // Reset Wi-Fi
  WiFi.mode(WIFI_OFF); // Reset Wi-Fi
  WiFi.mode(WIFI_STA); // Nastav Wi-Fi jako stanici
  WiFi.begin(ssid, heslo); // Zacni se pripojovat k Wi-Fi

  // Dokud nebudes pripojeny k Wi-Fi, vypisuj do seriove linky tecky
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
  setSyncProvider(ziskejInternetovyCas); // Casova knihovna bude pouzivat k synchronizaci tuto funkci
  setSyncInterval(300); // Casova knihovna bude synchronizovat cas kazdych 300 sekund
}


// Nekonecna smycka loop se spusti po zpracovani funkce setup a jeji obsah se opakuje stale dokola
void loop() {
  if (timeStatus() != timeNotSet) { // Zna uz casova knihovna cas?
    /* Smycku loop nechceme zbytecne blokovat prikazem delay,
     * a tak porovname aktualni cas s tim starym ulozenym v pomocne knihovne
     * Pokud se uz zmenil (pribyla sekunda), teprve tehdy
     * vypiseme formatovanou zpravu do seriove linky s aktualnim casem
    */
    if (now() != staryCas) {
      staryCas = now();

      char txtHodiny[21]; // Textovy retezec reprezentujici cas a datum
      sprintf(txtHodiny, "%02d:%02d:%02d %02d.%02d. %d",
              hour(),
              minute(),
              second(),
              day(),
              month(),
              year()); // Vytvoreni textu HH:MM:SS DD.MM. YYYY
      Serial.println(txtHodiny);
    }
  }
}

/* Funkce pro ziskani aktualniho casu z NTP serveru pomoci UDP protokolu
 * Tuto funkci si bude volat automaticky casova knihovna behem prubezne synchronizace
*/
time_t ziskejInternetovyCas() {
  while (Udp.parsePacket() > 0) ; // Pokud z UDP prichazeji nejaka data,zahod je
  IPAddress ntpServerIP;
  WiFi.hostByName(serverNTP, ntpServerIP); // Preved domenu casoveho serveru na IP adresu
  posliPaketNTP(ntpServerIP); // Odesli UDP davku na casovy server
  uint32_t cekej = millis();
  while (millis() - cekej < 1500) { // Cekej nejdele 1.5 sekundy na odpoved
    int size = Udp.parsePacket(); // Precti velikost odpovedi
    if (size >= 48) { // Odpoved musi mit nejmene 48 bajtu
      Udp.read(ntpPaket, 48); // Precti odpoved do pole ntpPacket
      /* 40-43 bajt odpovedi prepocitej na uplynule sekundy od roku 1900
       * a pripocitej take mistni casovou zonu (bezny cas: +1, letni cas +2)
      */
      unsigned long sekundyOd1900;
      sekundyOd1900 =  (unsigned long)ntpPaket[40] << 24;
      sekundyOd1900 |= (unsigned long)ntpPaket[41] << 16;
      sekundyOd1900 |= (unsigned long)ntpPaket[42] << 8;
      sekundyOd1900 |= (unsigned long)ntpPaket[43];
      return sekundyOd1900 - 2208988800UL + casovaZona * SECS_PER_HOUR;
    }
  }
  return 0;  // Pokud opdoved nedorazila do 1,5 sekundy, vrat nulu (sync se nepovedl)
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
