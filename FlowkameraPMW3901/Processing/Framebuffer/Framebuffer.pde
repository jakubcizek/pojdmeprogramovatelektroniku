// Vestavena knihovna pro praci se seriovou linkou
import processing.serial.*;

Serial serial; // Objekt spojeni skrze seriovou linku
String prichozi_data; // Promenna pro textova prichozi data
int[][] snimek = new int[35][35]; // Pole pro snimek 35x35 pixelu
boolean mame_novy_snimek = false; // Kontrolni pormenna, jestli uz mame cely novy snimek

// Hlavni funkce setup se jako v Arduinu spusti hned na zacatku
void setup() {
  size(700, 700); // Velikost okna
  // Nastartujeme seriovou linku s prvni mdostupnym portem (COM na Windows)
  // Predpokladame rychlost 500 000 b/s
  // Prichozi data se budou cachovat po radcich
  serial = new Serial(this, Serial.list()[0], 500000);
  serial.bufferUntil('\n');
  // Nadpis okna
  surface.setTitle("Flowkamera PMW3901 -- test čtení framebufferu");
}

// Funkce pro prekreslovani okna
void draw() {
  // Pokud mame novy snimek
  // nakreslime ho, pricemz kazdy jeho pixel bude predstavovat
  // ctverecek s rozmeru 20x20 pixelu
  if (mame_novy_snimek) {
    for (int y = 0; y < 35; y++) {
      for (int x = 0; x < 35; x++) {
        fill(snimek[y][x]);
        rect(x*20, y*20, 20, 20);
      }
    }
    // Resetujeme pomocnou promennou
    mame_novy_snimek = false;
  }
}

// Funkce, kterou Processing vola, pokud dorazi nejaka nova
// data po seriove lince
void serialEvent(Serial _serial) {
  // Data nam Processing cachuje p ocelych radcich,
  // takze precteme novy radek
  prichozi_data = _serial.readString().trim();
  // Rozdelime radek po mezerach a hodnoty prevedeme na cela cisla
  int[] hodnoty = int(split(prichozi_data, " "));
  // Prvni hodnota je cislo radku
  int cislo_radku = hodnoty[0];
  // Hodnot musi byt 35+1
  if (hodnoty.length == 36) {
    // Naplnime hodnotami konkretni radek nasi pameti pro snimek
    for (int x = 1; x < 36; x++) {
        snimek[cislo_radku][x-1] = hodnoty[x];
    }
  }
  // Pokud dorazil radek cislo 34, je to konec snimku, a tak pro kontrolu vypiseme
  // zpravu do konzole Processingu a dame vedet funkci draw, ze muze prekreslit okno 
  if(cislo_radku == 34){
    println("Snimek hotovy!");
    mame_novy_snimek = true;
  }
}
