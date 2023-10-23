// Knihovna pro praci se seriovou linkou
import processing.serial.*;

// Objekty obrazku barevnych a cernobilých opic
PImage[] opice = new PImage[6];

// Souradnice a rychlost pro tri detekovane objekty
int x[] = new int[3];
int y[] = new int[3];
int r[] = new int[3];

// Historie XY nasich objektu s hloubkou 100 pozic
int[][] historie_x = new int[3][100];
int[][] historie_y = new int[3][100]; 

// Vychozi Y bude 50 pixelu od spodniho okraje
// 1 px bude odpovidat 1 cm vzdalenosti
int posun_y = 50;

// Hlavni funkce setup se zpracuje hned na zacatku
void setup() {
  
  // Velikost okna, barva pozadi a titulek
  size(1280, 720);
  background(0);
  surface.setTitle("Radar LD2450");
  
  // Barevne opice
  opice[0] = loadImage("opice1.png");
  opice[1] = loadImage("opice2.png");
  opice[2] = loadImage("opice3.png");
  
  // Cernobile opice
  opice[3] = loadImage("opice1cb.png");
  opice[4] = loadImage("opice2cb.png");
  opice[5] = loadImage("opice3cb.png");
  
  // Otevreni prvniho dostupneho serioveho portu rychlosti 115200
  Serial serial = new Serial(this, Serial.list()[0], 115200);
  // Budeme ze seriove linky cist data po radcich
  serial.bufferUntil('\n');
  
  // Resetujeme historii
  for(int cil = 0; cil < 3; cil++){
    x[cil] = 0;
    y[cil] = 0;
    for (int i = 0; i < historie_x[cil].length; i++) {
      historie_x[cil][i] = x[cil];
      historie_y[cil][i] = y[cil];
    }
  }
}

// Funkce draw prekresluje platno
void draw() {    
  background(0);
  
  // Nejprve nakreslime trajektorie historie pohybu
  for(int cil = 0; cil < 3; cil++){
    for (int i = 1; i < historie_x[cil].length; i++) {     
      stroke(100, 100, 100);
      strokeWeight(1);
      line(width/2 + historie_x[cil][i-1], (height - posun_y) - historie_y[cil][i-1], width/2 + historie_x[cil][i], (height - posun_y) - historie_y[cil][i]);
    }
  }
  
  // A jeste jednou, ale tentokrat pro kresleni obrazku opicek
  // Chceme totiz, aby prekryly pripadnou trajektorii historie
  for(int cil = 0; cil < 3; cil++){
    imageMode(CENTER);
    // Pokud je XY != 0, opicka bude barevna 
    if(x[cil] + y[cil] != 0){
      image(opice[cil], width/2 + x[cil], (height - posun_y) - y[cil]); // Barevna opice
      fill(255); // Nastaví barvu textu na bilou
    }
    // V opacnem pripade cernobila - nedetekuje zadny pohyb tohoto cile
    else{
      image(opice[cil+3], width/2 + x[cil], (height - posun_y) - y[cil]); // Seda opice
      fill(100); // Nastaví barvu textu na sedou
    }
    
    // Textova statistika v rohu okna
    String stav = "Cil" + (cil+1) + ": X: " + x[cil] + " cm, Y: " + y[cil] + " cm, Rychlost: " + r[cil] + " cm/s";
    textSize(20);
    text(stav, 20, (25 * cil) + 50);
  }
}

// Zpracovani prichozich dat ze seriove linky
void serialEvent(Serial serial) {
  try{
    String radek = serial.readString().trim();
    println(radek);
    int[] data = int(split(radek, ","));
    if(data.length == 4){
      // Dekodkujeme id objektu/cile a jeho X, Y a rychlost
      int cil = data[0];
      x[cil] = data[1];
      y[cil] = data[2];
      r[cil] = data[3];
      
      // Aktualizujeme historii
      for (int i = historie_x[cil].length - 1; i > 0; i--) {
        historie_x[cil][i] = historie_x[cil][i-1];
        historie_y[cil][i] = historie_y[cil][i-1];
      }
      historie_x[cil][0] = x[cil];
      historie_y[cil][0] = y[cil];
    }
  }
  catch(Exception e){
    println(e);
  }
}

// Stisknuti R resetuje historii
void keyPressed() {
  if (key == 'R' || key == 'r') {
    for(int cil = 0; cil < 3; cil++){
      x[cil] = 0;
      y[cil] = 0;
      for (int i = 0; i < historie_x[cil].length; i++) {
        historie_x[cil][i] = x[cil];
        historie_y[cil][i] = y[cil];
      }
    }
  }
}
