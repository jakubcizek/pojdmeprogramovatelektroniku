// Vesta@vena knihovna pro praci se seriovou linkou
import processing.serial.*;

Serial serial; // Objekt spojeni skrze seriovou linku
String prichozi_data; // Promenna pro textova prichozi data
int x, y; // Souradnice XY dronu
// Historie souradnic XY pro 100 poslednich mereni
// Pouzivame pro kresleni trajektorie a grafu
int[] historie_x = new int[100];
int[] historie_y = new int[100]; 
// Objekt obrazku dronu
PImage dron;

// Hlavni funkce setup se jako v Arduinu spusti hned na zacatku
void setup() {
  size(1280, 720);  // Velikost okna
  colorMode(HSB, 360, 100, 100); // Budeme pracovat v barevnem rezimu HSB
  
  // Nastartujeme seriovou linku s prvni mdostupnym portem (COM na Windows)
  // Predpokladame rychlost 500 000 b/s
  // Prichozi data se budou cachovat po radcich
  serial = new Serial(this, Serial.list()[0], 500000);
  serial.bufferUntil('\n');
  // Nadpis okna
  surface.setTitle("Flowkamera PMW3901");
 
  // Vychozi souradnice dronu bude uprostred obrazovky
  x = width / 2;
  y = height / 2;
  
  // Resetujeme histoii naplnenim vsech hodnot na stred obrazovky
  for (int i = 0; i < historie_x.length; i++) {
    historie_x[i] = x;
    historie_y[i] = y;
  }
  
  // Nacteme obrazek dronu ze souboru s timto nazvem
  dron = loadImage("camera-drone.png");
}

// Funkce pro prekreslovani okna
void draw() {
  // Nastavime pzoadi okna na bilou
  // Pokud vas matou hodnoty, pripomenu,
  // ze nepracujeme v rezimu RGB, ale HSB,
  // ve kterem se nam bude lepe pracovat s barevnou skalou
  // od modre po rudou pro urceni relativni rychlosti
  background(0, 0, 100); 
  
  // Pomocna promenna pro urceni odstinu
  float maximalni_vzdalenost = 300;
  
  // Pomocne promenne pro kresleni grafu
  int maximalni_vyska_grafu = 100; 
  
  // Projdeme historii a nakreslime barevnou trajektorii a barevny graf
  for (int i = 1; i < historie_x.length; i++) {
    // Vypocet vzdalenosti mezi dvema body historie XY
    float vzdalenost = dist(historie_x[i-1], historie_y[i-1], historie_x[i], historie_y[i]);
    // Premapujeme vzdalenost 0-300 na hodnotu odstinu 270-0 (modra az cervena v modelu HSB)
    float odstin = map(vzdalenost, 1, maximalni_vzdalenost, 270, 0);
    
    // KRESLIME SEGMENT TRAJEKTORIE
    // Nastavime pero na odstin a nakreslime usecku segmentu
    stroke(odstin, 100, 100);
    strokeWeight(3);
    line(historie_x[i-1], historie_y[i-1], historie_x[i], historie_y[i]);
  
    // KRESLIME SLOUPEC GRAFU
    // Pri spodnim okraji okna budeme kreslit sloupcovy graf
    // Vyska a barva sloupce odpovida rychlosti/velikosti zmeny XY v danem segmentu
    float vyska = map(vzdalenost, 0, maximalni_vzdalenost, 0, maximalni_vyska_grafu);
    float sloupec_x = map(i, 0, historie_x.length, 0, width);  
    float sloupec_y = height - vyska;
    float sirka_sloupce = width / float(historie_x.length);
    // Nastavime vyplnovaci stetec na odstin a nakreslime sloupec pro danou hodnotu
    fill(odstin, 100, 100);
    noStroke();
    rect(sloupec_x, sloupec_y, sirka_sloupce, vyska);
  }
  
  // Nakrslime obrazek dronu na nove pozici XY
  imageMode(CENTER);
  image(dron, x, y);
}

// Funkce, kterou Processing vola, pokud dorazi nejaka nova
// data po seriove lince
void serialEvent(Serial _serial) {
  // Data nam Processing cachuje po celych radcich,
  // takze precteme novy radek
  prichozi_data = _serial.readString().trim();
  // Rozdelime radek po mezerach a hodnoty prevedeme na cela cisla
  int[] souradnice = int(split(prichozi_data, " "));
  // Jen pro kontrolu, ze mame prave dve hodnoty se zmenou X a Y
  if (souradnice.length >= 2) {
    // Aktualizujeme titulek okna
    surface.setTitle("Flowkamera PMW3901: " + souradnice[0] + ", " + souradnice[1]);
    // pripocteme zmeny XY k aktualnim hondotam souradnic XY
    x -= souradnice[0];
    y -= souradnice[1]; 
    // Omezime hodnoty XY jen na rozmery okna,
    // at nam dron nevyleti za jeho okraj
    x = constrain(x, 0, width);
    y = constrain(y, 0, height);
    // Aktualizujeme historii souradnic XY pro kresleni trajektorie a grafu
    aktualizujHistorii();
  }
}

// Pri stisku klavesy R resetujeme souradnice XY na stred obrazovky
// a stejne tam smazeme historii (takze ji nastavime na stred ve vsech bodech)
void keyPressed() {
  if (key == 'R' || key == 'r') {
    x = width  /2;
    y = height / 2;
    for (int i = 0; i < historie_x.length; i++) {
      historie_x[i] = x;
      historie_y[i] = y;
    }
  }
}

// Funkce pro aktualizaci pole s historii souradnic
void aktualizujHistorii() {
  for (int i = historie_x.length - 1; i > 0; i--) {
    historie_x[i] = historie_x[i-1];
    historie_y[i] = historie_y[i-1];
  }
  historie_x[0] = x;
  historie_y[0] = y;
}
