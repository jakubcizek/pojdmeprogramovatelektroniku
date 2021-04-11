/*
 * Skript pro běhové prostředí Processing
 * Stahnete zde: https://processing.org/download/

 * Program se skrze seirovou linku/USB spoji s mikrokontrolerem,
 * na kterem se nesutale pomoci servomotoru otaci laserovy dalkomer
 * v rozsahu 0-180 uhlovych stupnu.

 * Po kazde zmerene vzdalenosti pro dany uhel mikrokontroler odesle udaj
 * skrze seriovou linku do PC, kde ji tento program vykresli pomoci
 * polarnich souradnic do grafu.
*/

// Pouzijeme knihovnu pro praci se seriovou linkou
import processing.serial.*;

// Pomocne promenne pro velikost okna
int OKNO_SIRKA = 1000;
int OKNO_VYSKA = 500;
int OKNO_OKRAJ = 40;

// Nejvetsi vzdalenost jako pomocna hodnota pro meritko – zoom
// Lze libovolne menit, ve vychozim stavu nastaveno na 400 cm
int VZDALENOST_MAX = 400;

// Tloustka bodu predstavujici udaj z dalkomeru pro dany uhlovy stupen
int BOD_TLOUSTKA = 5;

// Souradnice X a Y na grafu
float x;
float y;

// Pole vzdalenosti pro uhlove stupne 0 az 180
int[] vzdalenosti = new int[181];

// Objekt serioveho spojeni
Serial port;

// Funkce pro ziskani polarnich souradnic X a Y pro dany uhlovy
// stupen a zmerenou vzdalenost k nejblizsi prekazce v danem miste
void ziskejSouradnice(int uhel, int vzdalenost){
  x = (float)((OKNO_SIRKA / 2) + (Math.cos((Math.PI/180) * uhel) * map(vzdalenost, 0, VZDALENOST_MAX, 0, (OKNO_SIRKA / 2) - (OKNO_OKRAJ * 2))));
  y = (float)(OKNO_OKRAJ + (Math.sin((Math.PI/180) * uhel) * map(vzdalenost, 0, VZDALENOST_MAX, 0, OKNO_VYSKA - (OKNO_OKRAJ * 2))));
}

// Nakreseni pulkruhu se stupnici 0 az 180 stupnu
void nakresliStupnici(){
  float sx;
  float sy;
  fill(0,255, 0);
  textSize(15);
  for(int i = 0; i <= 180; i++){
    if(i % 10 == 0){
      sx = (float)((OKNO_SIRKA / 2) + (Math.cos((Math.PI/180) * i) * ((OKNO_SIRKA / 2) - (OKNO_OKRAJ * 2))));
      sy = (float)(OKNO_OKRAJ + (Math.sin((Math.PI/180) * i) * (OKNO_VYSKA - (OKNO_OKRAJ * 2))));
      text(180-i, sx, (OKNO_VYSKA - sy));
    }
  }
}

// Funkce pro vymazani pole se vzdalenostmi po kazdy z uhlovych stupnu
void resetujVzdalenosti(){
  for(int i = 0; i <= 180; i++){
    vzdalenosti[i] = 0;
  }
}

// Funkce pro vypsani textoveho popisku do okna programu
void popisek(){
  fill(0,255, 0);
  textSize(30);
  text("2D Lidar", 20, 20 + textAscent());
  text((int)frameRate + " fps", OKNO_SIRKA - 120, 20 + textAscent());
  textSize(15);
  text("Benewake TF-Luna ToF", 20, 60 + textAscent());
  text("Arduino Uno, analogové servo", 20, 80 + textAscent());
  text("Časopis Computer", 20, 100 + textAscent());
  
}

// Hlavni funcke setup, ktera se spusti hned na zacatku podobne jako na Arduinu
void setup(){
  size(1000,500); // Velikost okna programu
  background(0); // Cerne pozadi
  stroke(255); // Bila barva kresliciho pera
  strokeWeight(1); // Tlosutka pera 1 px
  fill(0); // Cerna barva vyplne
  surface.setTitle("2D Lidar časopisu Computer"); // Titulek okna programu
  resetujVzdalenosti(); // Vymazani pole vzdalenosti
  
  // Nastartovani seriove linky na rychlost 115 200 b/s
  // a volba prvniho serioveho zarizeni v poradi. Mikrokontroler
  // v tuto dobu uz musi byt skrze USB pripojeny k PC
  port = new Serial(this, Serial.list()[0], 115200);
  port.clear(); // Reset pametoveho bufferu seriove linky
}

// Funkce draw se podoba funkci loop z Arduina,
// takze se opakuje stale dokola a kresli do okna nas obsah
void draw(){
  background(0); // Vypln okno cernou barvou
  while(port.available() > 0){ // Pokud ze seriove linky prichazeji nejaka data
    String radek = port.readStringUntil('\n'); // precti je jako text az ke konzi radku
    if(radek != null){ // Pokud jsme dokazali precist cely radek
      radek = radek.trim(); // Smaz z nej pripadne neviditelne znaky
      // Radek ma format UHEL VZDALENOST,
      // najdeme tedy pozici mezery
      int mezera = radek.indexOf(' ');
      if(mezera > 1){
        try{
          // Podle pozice mezery ziskame cislenou hodnotu uhlu a vzdalenosti
          int uhel = Integer.valueOf(radek.substring(0, mezera));
          int vzdalenost = Integer.valueOf(radek.substring(mezera+1));
          vzdalenosti[uhel] = vzdalenost; // Ulozime hodnoty do naseho pole
        // Kdyby se to nepodarilo, tento radek zahodime
        // Tzn., kdyby radek neobsahoval ciselne hodnoty atp.
        }catch(Exception e){}
      }
    }
  }
  
  // Ted vykreslime hodnoty z pole jako bile body
  fill(255);
  for(int i = 0; i <= 180; i++){
    if(vzdalenosti[i] > 0){
      ziskejSouradnice(i, vzdalenosti[i]);
      circle(x, (OKNO_VYSKA - y), BOD_TLOUSTKA);
    }
  }
  
  // Nakreslime stupnici
  nakresliStupnici();
  
  // Nakreslime popisky
  popisek();
  
  // A to je konec pruchodu funkce draw
  // Cili vse muze zacit znovu
}
