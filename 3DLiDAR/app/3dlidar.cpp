// Hlavickove soubory knihovny OpenCV
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

// Hlavicka pro detekci stisku klavesove zkrakty
// pro vynucene ukonceni programu CTRL+C
#include <signal.h>

// Hlavicka s ULD API cipu VL53L5CX
#include "vl53l5cx_api.h"

// C++ namespace knihovny OpenCV
using namespace cv;

// Pomocna promenna se stavem hlavni smycky programu
int loop = 1;

// Po stisku CTRL+C zpracuj tuto funkci
void onSignal(int signal){
	(void) signal;
	printf("\r\nBezpecne ukoncuji program...\r\n");
	// Nastav pomocnou promennou stavu smycky na nulu
	// Tim se smycka ukonci a s ni i program
	loop = 0;
}

// Obdoba stejnojmenne funkce z Arduina
// https://www.arduino.cc/reference/en/language/functions/math/map/
// Prepocitame hodnotu x z rozshau in_min/in_max na novy rozsah out_max/out_max
// Poslouzí pro vypocet intenzity zeleneho kanalu v hloubkove mape,
// Kde kazde vzdalenosti odpovida jina sytost zelene barvy
long map(long x, long in_min, long in_max, long out_min, long out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Funkce pro nakresleni obdelniku pomoci OpenCV
// Promenna weight predstavuje intenzitu zelene barvy obdelniku
void filledRect(Mat img, uint8_t weight, Point tl, Point br){
  rectangle(
	  img,
	  tl,
	  br,
	  Scalar(0, weight, 0),
	  -1,
	  LINE_8
  );
}

// Hlavni funkce programu MAIN
// Tady vse zacina
// Obdoba funkce setup ve svete Arduina
int main(int argc, char *argv[]){
	// Nepouzivame parametry prikazove radky
	(void) argc;
	(void) argv;
	uint8_t ready; // Pomocna promenna pro zjisteni, jestli uz mame nova data
	VL53L5CX_Configuration vl53l5cx; // Struktura s konfiguraci dalkomeru
	VL53L5CX_ResultsData results; // Struktura s vysledky mereni
	uint8_t rozliseni = VL53L5CX_RESOLUTION_4X4; // Rozliseni cidla, 4x4 segmentu
	uint8_t rezim = VL53L5CX_RANGING_MODE_CONTINUOUS; // Rezim mereni, kontinualni
	int16_t vzdalenosti[16] = {}; // Pomocne pole s 16 vzdalenostmi pro rozliseni 4x4

	signal(SIGINT, onSignal); // Registrace funkce onSignal pri stisku CTRL+C (signal SIGINT)


	// Nastartovani 1. kamery v poradi skrze OpenCV
	VideoCapture cap(0);
        if (!cap.isOpened()){
        	printf("Nelze otevrit RPi kameru\r\n");
        	return -1;
        }

	// Inicializace sbernice I2C
	// A prvni pokus spojeni s dalkomerem na adrese 0x29
	printf("Oteviram I2C sbernici i2c1\r\n");
	if(vl53l5cx_comms_init(&vl53l5cx.platform)){
		printf(" CHYBA\r\n");
		return -1;
	}

	// Nahrani ULD firmwaru skrze I2C do operacni mapemti RAM cidla VL53L5CX
	printf("Kopiruji ULD %s do RAM VL53L5CX... ", VL53L5CX_API_REVISION);
	if(vl53l5cx_init(&vl53l5cx)){
		printf("CHYBA\r\n");
		return -1;
	}
	else printf("OK\r\n");

	// Nastaveni rozliseni mereni
	// A vytvoreni odpovidajiciho snimku pro OpenCV
	uint8_t sloupce = (uint8_t)sqrt(rozliseni); // Zjisteni poctu sloupcu z roliseni (odmocnina rozliseni)
	printf("Nastavuji rozliseni na %dx%d... ", sloupce, sloupce);
	if(vl53l5cx_set_resolution(&vl53l5cx, rozliseni)) printf("CHYBA\r\n");
	else printf("OK\r\n");
	Mat lidar(sloupce * 100, sloupce * 100, CV_8UC3, Scalar(0, 0, 0));

	// Nastaveni frekvence mereni na 1 Hz
	printf("Nastavuji frekvenci mereni na %d Hz ", 1);
	if(vl53l5cx_set_ranging_frequency_hz(&vl53l5cx, 1)) printf("CHYBA\r\n");
	else printf("OK\r\n");

	// Nastaveni rezimu mereni
	printf("Nastavuji rezim mereni na Continuous ");
	if(vl53l5cx_set_ranging_mode(&vl53l5cx, rezim)) printf("CHYBA\r\n");
	else printf("OK\r\n");

        // Spusteni mereni
	printf("\r\n*** Zacatek mereni (vzdalenosti v mm) ***\r\n");
	vl53l5cx_start_ranging(&vl53l5cx);

	uint32_t counter = 0; // Pocitadlo jednotlivych mereni

	// START HLAVNI SMYCKY PROGRAMU
	// Obdoba funkce loop v Arduinu
	while(loop){
		Mat frame; // Snimek kamery
         	cap.read(frame); // Precteni snimku z kamery
		namedWindow("Kamera",1); // Nazev okna pro snimek kamery
		namedWindow("Lidar", 1); // NAzev okna pro hloubkovou mapu z dalkomeru

		vl53l5cx_check_data_ready(&vl53l5cx, &ready); // Kontrola, jestli uz mame data z dalkomeru
		if(ready){ // Pokud ano...
			printf("\r\n%d. mereni", counter);
			vl53l5cx_get_ranging_data(&vl53l5cx, &results); // Uloz vysledky do strukutry results
			// Pomocne promenne pro urceni radku a sloupcu
			int8_t radek = -1;
			int8_t sloupec = 0;
			// Pomocne promenne pro nalezeni MAX, MIN
			int16_t max = 0;
			int16_t min = 9999;
			// Projdi pole s namerenymi vzdalenostmi v jednotlivych segmentech
			// Pouzivame rozliseni 4x4, takze jich bude 16
			for(uint8_t i = 0; i < rozliseni; i++){
				int16_t vzdalenost = results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * i];
				// Socitej MAX a MIN hodnotu
				if(vzdalenost > max) max = vzdalenost;
				if(vzdalenost < min) min = vzdalenost;
				// Pridej vzdalenost do pole vzdalenosti
				vzdalenosti[i] = vzdalenost;
			}

			// Mame 16 vzdalenosti, takze je znovu projdeme
			// A tentokrat je uz vykreslime jako tabulku sloupci a radku
			for(uint8_t i = 0; i < rozliseni; i++){
				if(i % sloupce == 0){
					printf("\r\n");
					radek++;
					sloupec = 0;
				}
				printf("%04dmm\t", vzdalenosti[i]);

				// Orezani hodnota, aby byla nejmensi hodnota 1 a nejvyssi 4000
				if(vzdalenosti[i] < 1) vzdalenosti[i] = max;
				if(vzdalenosti[i] > 4000) vzdalenosti[i] = max;
				// Spocitani intenizty zelene (255-0) podle rozsahu (min-max)
				uint8_t intenzita = map(vzdalenosti[i], min, max, 255, 0);
				// Nakresleni zeleneho segmentu do snimku lidar
				filledRect(lidar, intenzita, Point(sloupec*100, radek*100), Point((sloupec*100)+100,(radek*100)+100));
				sloupec++;
			}
			printf("\r\n");
			counter++;
		}
		// Zobrazeni snimku frame (kamera) a lidar (hloubkova mapa)
		// pomci knihovny OpenCV
		imshow("Kamera", frame);
		imshow("Lidar", lidar);

		// Detekce stisku klavesy ESC, ktera ukonci program
		// Druha moznost vedle CTRL-C v textovem terminalu
		char c = (char)waitKey(25);
        	if(c == 27) loop = 0;

		// Pockej 5 ms a opakuj
		// Abychom nezahltili neustalym dotazovanim,
		// jestli uz ma data (polling)
		WaitMs(&vl53l5cx.platform, 5);
	}

	// Pokud jsme po stisku ESC, nebo CTRL-C vyskocili z hlavni smycky,
	// korektne ukoncime spojeni s kamerou a dalkomerem a
	// ukoncime program
	cap.release();
    	destroyAllWindows();
	vl53l5cx_stop_ranging(&vl53l5cx);
	vl53l5cx_comms_close(&vl53l5cx.platform);
	return 0;
}
