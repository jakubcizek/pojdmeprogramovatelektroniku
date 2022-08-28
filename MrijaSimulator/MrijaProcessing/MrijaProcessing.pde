// https://processing.org/reference/libraries/sound/index.html
import processing.sound.*;

// https://github.com/alexandrainst/processing_websockets
import websockets.*;

// IP adresa a TCP port našeho letadla, tedy Wi-Fi čipu ESP32
String ipAdresaLetadla = "172.17.16.131";
int tcpPortLetadla = 80;

// Proměnné pro souřadncie kamery, obrázku pozadí a 3D modelu letadla
float x,y,z;
PImage pozadi;
PShape letadlo;

// Proměnné s úhly z gyroskopu
float xGyro = 0;
float yGyro = 0;
float zGyro = 0;

// Souřadncie letadla ve scéně
float pX = 0.0;
float pY = 4300;

// Soubor se zvukovou nahrávkou letadla
SoundFile file;

// Pomocné proměnné pro stav linky a její statistiku
boolean dataOk = false;
boolean spojeniOk = false;
int dataPocet = 0;

// Objekt websocketového klientu
WebsocketClient websocket;

// Otáčením kolečka myši přiblížíme, nebo vzdálíme kameru
// a tedy i model letadla
void mouseWheel(MouseEvent event) {
  z += event.getCount();
}

// Tato funkce se bude volat, jakmile z WebSocketu dorazí nějaká textová data
void webSocketEvent(String data){
  if(data != null){
    // Zprávy se znakem # na začátku neobsahují souřadnice
    if(data.charAt(0) == '#'){
      data = data.substring(1);
      println(data);
      // Touto zprávou websocketový server přivítá každého nového klienta
      // Jedná se tedy o ověření, že spojení funguje 
      if(data.equals("Mrija Websocket Server te vita!")) spojeniOk = true;
    }
    // Pokud zpráva neobsahuje na začátku znak #,
    // musí to být trojice úhlů X, Y a Z ve formátu:
    // XX.XXXX,YY.YYYY,Z.ZZZZ
    // Dekódujeme tyto hodnoty a uložíme do proměnných
    else{
      String[] casti = split(data, ',');
      try{
      xGyro = -1 * Float.parseFloat(casti[0]);
      yGyro = Float.parseFloat(casti[1]);
      zGyro = Float.parseFloat(casti[2]);
      dataOk = true;
      dataPocet++;
      }
      catch(Exception e){
        dataOk = false;
        println("Nemohu dekódovat úhly X, Y, Z ");
      }
    }
  }
}

// Funkce setup se zpracuje hned na začátku
// Analogie funkce setup ze světa Arduino
void setup() {
  surface.setTitle("Živě.cz Mrija Simulátor 2022");
  surface.setResizable(false);
  // Vytvoříme 3D scénu v okně s rozměry 1280x720 pixelů
  size(1280, 720, P3D);
  // Souřadnice kamery
  x = width/2;
  y = height/2;
  // Ve výchozím stavu bude kamera hluboko ve scéně
  // Na začátku ji postupně posuneme dozadu,
  // čímž vytvoříme animaci přilétající Mriji
  z = 600;
  // Nastavíme obrázek pozadi okna,
  // nahrajeme 3D model Mriji a začneme přehrávat zvuk motorů
  pozadi = loadImage("brno.jpg");
  letadlo = loadShape("mrija.obj");
  file = new SoundFile(this, "motory.wav");
  file.play();
  
  // Otevřeme spojení s websocketovým serverem
  // na stanovené IP adrese a TCP portu
  try{
    websocket = new WebsocketClient(this, "ws://" + ipAdresaLetadla + ":" + tcpPortLetadla);
  }
  catch(Exception e){
    dataOk = false;
    println("Nemohu dekodovat uhly X, Y, Z ");
  } 
}

// Funkce draw stále dokola překresluje okno
// Je to tedy analogie funkce loop ze světa Arduino
void draw() {
  // Aktualizujeme nadpis okna, ve kterém vypisujeme FPS
  surface.setTitle("Mrija Simulátor 2022 (" + round(frameRate) + " fps)");
  // Pokud nehraje stopa s hlukem motorů, začni ji přehrávat
  if(!file.isPlaying()) file.play();
  // Jako pozadí okna nastav fotografii Brna
  background(pozadi);
  // Nastav pohled kamery na souřadnice X, Y, Z
  translate(x,y,z);
  
  // Pokud je hloubka (z) modelu větší než 0,
  // je model skrytý vpředu mimo záběr, a tak
  // jej začnu vysunovat do záběru. Vznikne animace
  // úvodního příletu letadla do středu obrazovky
  if(z > 0){ 
    z--;   
  }
  
  // Natoč scénu podle aktuální hodnoty X, Y, Z
  // z gyroskopu. Stupně přervedeme na radiány
  rotateX(radians(xGyro));
  rotateY(radians(zGyro));
  rotateZ(radians(yGyro));
  
  // Tvorba komplexního modrého bodového světla
  // Osvit modelu zhruba ze zadního pravého rohu
  // podobně jako na podkladové fotografii
  pointLight(3, 177, 252,  0,  -500,  600);
  pointLight(3, 177, 252,  0,  500,  600);
  pointLight(3, 177, 252,  1000,  0,  0);
  
  // Měřítko modelu
  scale(0.02);
  // vykreslení modelu na souřadnice X, Y
  shape(letadlo, pX, pY);
  
  // 2D vrstva s textovými poisky
  camera();
  hint(DISABLE_DEPTH_TEST);
  noLights();
  textMode(MODEL);
  textSize(20);
  text("Leť bezpečně, jsi nad Brnem!", 10, 30);
  textSize(15);
  text("Naklonění okolo osy Y (klopení, pitch): " + (xGyro) + "°" , 10, 60);
  text("Naklonění okolo osy X (klonění, roll): " + (yGyro) + "°" , 10, 80);
  text("Otáčení okolo osy Z (bočení, yaw): " + (zGyro) + "°" , 10, 100);
  text("Letadlo X: " + (pX), 10, 120);
  text("Letadlo Y: " + (pY), 10, 140);
  text("Kamera X: " + (x), 10, 160);
  text("Kamera Y: " + (y), 10, 180);
  text("Kamera Z: " + (z), 10, 200);
  
  // Barevné indikátory v pravém horním rohu
  // Websocket spojení zelená/červená
  stroke(0,0,0);
  if(spojeniOk)fill(0,255,0);
  else fill(255,0,0);
  rect(width-140, 20, 15, 15);
  fill(0,0,0);
  text(ipAdresaLetadla, width-110, 32);
  
  // Korektní data čevená/zelená
  //Celkový počet přijatých úhlů X, Y, Z
  if(dataOk)fill(0,255,0);
  else fill(255,0,0);
  rect(width-140, 45, 15, 15);
  fill(0,0,0);
  text(dataPocet, width-110, 57);
  
  hint(ENABLE_DEPTH_TEST);
}
