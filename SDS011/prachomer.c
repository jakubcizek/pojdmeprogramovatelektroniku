/*

Program Prachomer

Zakladni klient pro prachovy senzor SDS011 pripojeny k pocitaci skrze seriovou linku/USB 
Program predpoklada, ze prachomer pracuje ve vychozim automatickem 1Hz stavu

Pro spravnou funkci usinani a probouzeni je treba upravit ID cipu a kontrolni soucty
v instrukcich pro tyto akce 

Na Linuxu/Raspberry Pi prelozite treba prikazem:
gcc prachomer.c -o prachomer

Pote spustte napovedu prikazem ./prachomer -h

*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>

// Pomocna promenna, jestli se maji za beh uvypisovat podrobnejsi informace
uint8_t vypisovat_podrobnosti = 0;

// Seznam funkci
uint8_t checksum(uint8_t data[], uint8_t delka, uint8_t zacatek, uint8_t konec);
uint8_t zapis_bajt(int fd, uint8_t b);
uint8_t precti_bajt(int fd, uint8_t* ok);
int otevri_port(const char* port);
void zavri_port(int f);
int8_t precti_koncentraci(int fd, float* pm25, float* pm10, const uint32_t limit_us, uint32_t* doba);
void uspat_senzor(int fd);
void probudit_senzor(int fd);
void napoveda();
int najdi_parametr(int argc, char* argv[], const char* parametr);


// Funcke pro vypocet 8bitoveho kontrolniho souctu
uint8_t checksum(uint8_t data[], uint8_t delka, uint8_t zacatek, uint8_t konec){
    uint8_t _checksum = 0;
    for(uint8_t i=zacatek; i <= konec; i++) _checksum += data[i];
    return _checksum;
}

// Funkce pro zapis bajtu do serioveho/USB zarizeni
uint8_t zapis_bajt(int fd, uint8_t b){
    uint8_t buffer[1];
    buffer[0] = b;
    return write(fd, buffer, 1);
}

// Funcke pro precteni bajtu ze serioveho/USB zarizeni
uint8_t precti_bajt(int fd, uint8_t* ok){
    uint8_t buffer[1] = {0};
	*ok = read(fd, buffer, 1);
    return buffer[0];
}

// Funkce pro otevreni serioveho/USB portu
// SDS011 pracuje pri rychlosti 9 600 b/s
int otevri_port(const char* port){
    int fd;
    struct termios config;

	printf("Pokouším se otevřít %s... ", port);
	fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
	fcntl(fd, F_SETFL, 0);
	tcgetattr(fd, &(config));
	cfmakeraw(&(config));

	config.c_cflag = CS8 | CLOCAL | CREAD;
	config.c_cflag |= B9600;
	config.c_cc[VMIN] = 0;
	config.c_cc[VTIME] = 1;
	config.c_cflag &= ~(PARODD | PARENB);
	config.c_cflag &= ~CSTOPB;
	config.c_iflag &= ~(IXON | IXOFF | IXANY);

	if (tcsetattr(fd, TCSANOW, &(config))){
		printf("CHYBA!\r\n");
		printf("Vybrali jste správné zařízení?\r\nKomunikuje rychlostí 9600 b/s a s konfigurací 8n1?\r\n\r\n");
        napoveda();
		exit(1);
	}
	printf("OK (%d)\r\n", fd);
    return fd;
}

// Funkce pro zavreni serioveho portu
void zavri_port(int f){
    close(f);
}

// Funkce pro slepe precteni koncentrace (cip v rezimu automatickeho zasilani zprav)
// Funkce blokuje beh nejvyse po dobu limit_us mikrosekund (bezpecna hodnota je 1,5s pri 1Hz zasilani zprav)
int8_t precti_koncentraci(int fd, float* pm25, float* pm10, const uint32_t limit_us, uint32_t* doba){
    int8_t vysledek = -1;
    uint8_t stav = 0;
    uint8_t pocitadlo = 2;
    uint64_t _doba = 0;
    uint64_t nyni = 0;
    uint8_t ok = 0;
    uint8_t bajt = 0;
    uint64_t _limit = (uint64_t)limit_us * 1000U;
    struct timespec ts_start;
    struct timespec ts_nyni;

    // Zjisteni uvodniho casu v nanosekundach
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    uint64_t start = (uint64_t)ts_start.tv_sec * 1000000000U + (uint64_t)ts_start.tv_nsec;
 
    // Kostra zpravy, kterou chceme precist
    uint8_t zprava[10] = {0xAA, 0xC0, 0, 0, 0, 0, 0, 0, 0, 0xAB};

    // Nekonecna smycka, kterou ukonci timeout
    while(_doba <= _limit){
        // Zjisteni aktualniho casu v nanosekundach
        clock_gettime(CLOCK_MONOTONIC, &ts_nyni);
        nyni = (uint64_t)ts_nyni.tv_sec * 1000000000U + (uint64_t)ts_nyni.tv_nsec;
        // Vypocet doby behu smycky jako rozdilu mezi aktualnim a uvodnim casem
        _doba = (nyni - start);
        // Precti bajt ze seriove linky
        // Do promenne ok se ulozi pocet prectenych bajtu (pri uspechu musi byt rovna 1)
        bajt = precti_bajt(fd, &ok);

        // Pokud bajt odpovida zacatku hlavicky, zmen promennou stav
        if(bajt == 0xAA) stav = 1;
        else if(stav == 1 && bajt == 0xC0){
            stav = 2;
            pocitadlo = 2;
        }
        // Postupne upravuj promennou stav, ktera hlida stav cteni
        else if(ok == 1 && stav == 2 && pocitadlo == 2) stav = 3;
        else if(ok == 1 && stav == 3 && pocitadlo == 9 && bajt == 0xAB) stav = 4;

        // Pkud je stav roven 3, uz jsem precetl hlavicku,
        // takze mohu zacit cist samotna data
        if(stav == 3 && ok == 1){
            zprava[pocitadlo++] = bajt;
        }

        // Pokud je stav roven 4, uz jsme precetl paticku AB
        // a muzu vypocitat vysledek
        if(stav == 4){
            // Volitelne kontrolni vypsani cele zpravy
            if(vypisovat_podrobnosti){
                printf("*********************************\r\n* ");
                for(uint8_t i=0;i<10;i++) printf("%02X ", zprava[i]);
                printf("*\r\n*********************************\r\n");
            }
            // Pokud neodpovida kontrolni socuet, vrat chybu
            if((checksum(zprava, sizeof(zprava), 2, 7) != zprava[8])){
                vysledek = 0;
                break;
            }
            // Pokud kontrolni soucet odpovida
            else{
                // Spoj bajty v 16bit hodnoty
                uint16_t _pm25 = (zprava[3] * 256) + zprava[2];
                uint16_t _pm10 = (zprava[5] * 256) + zprava[4];
                // Ziskej finalni hodnoty koncentraci
                *pm25 = (float)_pm25/10.0f;
                *pm10 = (float)_pm10/10.0f;
                // Pokud cidlo zmerilo divne hodnoty vyssi nez ty hranicni,
                // vrat chybu
                if(*pm25 > 1000.0f || *pm10 > 2000.0f){
                    vysledek = -2;
                // Anebo vrat korektni vysledek funkce 1
                }else{
                    vysledek = 1;
                }
                // Ukonči nekonečnou smyčku
                break;
            }
        }
    }
    // Vrať dobu zpracovavani smycky v mikrosekundach
    *doba = (uint32_t) (_doba/1000U);
    // Vrat vysledek
    return vysledek;
}

// Funkce pro uspani cidla
// Pozor, instrukce bude fungovat jen na cidlu s ID 2E81
// Musite zpravu upravit podle sveho ID vcetne kontrolniho souctu v predposlednim bajtu
void uspat_senzor(int fd){
    uint8_t spanek[19] = {
        0xAA, 0xB4, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x2E,	0x81, 0xB6, 0xAB
    };
    for(uint8_t i=0;i<19;i++){
        if(vypisovat_podrobnosti) printf("Odesilam bajt %02X ... ", spanek[i]);
        uint8_t ok = zapis_bajt(fd, spanek[i]);
        if(vypisovat_podrobnosti){
            if(ok) printf("OK\r\n");
                else printf("CHYBA\r\n");
        }
    }
}

// Funkce pro probuzeni senzoru
void probudit_senzor(int fd){
    uint8_t budicek[19] = {
        0xAA, 0xB4, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x2E,	0x81, 0xB7, 0xAB
    };
    for(uint8_t i=0;i<19;i++){
        if(vypisovat_podrobnosti) printf("Odesilam bajt %02X ... ", budicek[i]);
        uint8_t ok = zapis_bajt(fd, budicek[i]);
        if(vypisovat_podrobnosti){
            if(ok) printf("OK\r\n");
                else printf("CHYBA\r\n");
        }
    }
}

// Funkce pro zobrazeni napovedy
void napoveda(){
    printf("Priklad pouziti:\r\n");
    printf(" prachomer -d /dev/ttyUSB0\r\n\r\n");
    printf("Volitelne parametry:\r\n");
    printf(" Pocet mereni: -c N\r\n");
    printf(" Max. doba cekani na vysledek (us): -t N\r\n");
    printf(" Vypisovat podrobnosti: -l\r\n");
    printf(" Uspat senzor: -s\r\n");
    printf(" Probudit senzor: -b\r\n");
}

// FunkCe pro nalezeni parametru programu
int najdi_parametr(int argc, char* argv[], const char* parametr){
    for(uint8_t i=0; i<argc; i++){
        if(strcmp(argv[i], parametr) == 0){
            return i;
        }
    }
    return -1;
}

// ***********************
// HLAVNI FUNKCE PROGRAMU 
// ***********************
int main(int argc, char* argv[]){

    // Pokud chybi jakekoliv parametry,
    // zobraz napovedu a skonci
    if(argc == 1){
        napoveda();
        return 0;
    }

    int fd;
    float pm25;
    float pm10;
    uint32_t doba;
    uint8_t opakovani = 1;
    uint32_t limit_us = 1500000;

    // Pokud je v parametrech -l, aktivuje podrobnejsi vypisovani informaci
    if(najdi_parametr(argc, argv, "-l") > 0){
        vypisovat_podrobnosti = 1;
        printf("Zapinam rezim vypisovani podrobnosti\r\n");
    }

    // Najdi parametr -d pro cestu k seriovemu zarizeni
    // Pri pouziti USB prevodniku to bude na Raspberry Pi
    // pravdepodobne /dev/ttyUSB0, /dev/ttyUSB1 atp.
    int pozice = najdi_parametr(argc, argv, "-d");
    // Pokud jsem nasel parametr -d, otevri seriove spojeni
    if(pozice > 0) fd = otevri_port(argv[pozice+1]);
    else{
        // V opacnem pripade zobraz napovedu a skonci
        napoveda();
        return 0;
    }


    // Vyhledej ostatni parametry nezavisle na poradi
    for(uint8_t i=3; i<argc; i++){
        if(strcmp(argv[i], "-c") == 0){
            opakovani = atoi(argv[i+1]);
            if(vypisovat_podrobnosti) printf("Nastavuji pocet mereni na %d\r\n", opakovani);
            i++;
        }

        if(strcmp(argv[i], "-t") == 0){
            limit_us = strtoll(argv[i+1], NULL, 10);
            if(vypisovat_podrobnosti) printf("Nastavuji max. dobu cekani na vysledek na %lu us\r\n", limit_us);
            i++;
        }

        if(strcmp(argv[i], "-s") == 0){
            if(vypisovat_podrobnosti) printf("Uspavam senzor!\r\n");
            uspat_senzor(fd);
        }

        if(strcmp(argv[i], "-b") == 0){
            if(vypisovat_podrobnosti) printf("Probouzim senzor!\r\n");
            probudit_senzor(fd);
        }

        if(strcmp(argv[i], "-h") == 0){
            napoveda();
        }
    }

    // Podle poctu opakovani ve smycce zachyt seriove zpravy a vypis hodnoty do prikazove radky
    for(uint8_t i=0; i<opakovani; i++){
        int8_t vysledek = precti_koncentraci(fd, &pm25, &pm10, limit_us, &doba);
        if(vysledek == 1){
            printf("PM2.5: %.2f, PM10: %.2f, doba: %lu us\r\n", pm25, pm10, doba);
        }
        // V pripade chyby podle chyboveho kodu vypis chybu
        else{
            switch(vysledek){
                case -2: printf("Chybna detekce, hodnoty jsou abnormalne vysoke!\r\n"); break;
                case -1: printf("Prekrocen casovy limit!\r\n"); break;
                case  0: printf("Chybna seriova data, checksum neodpovida!\r\n"); break;
                default: printf("Neznama chyba %d\r\n", vysledek);
            }
        }
    }

    // Zavri seriove spojeni
    zavri_port(fd);

    // Ukonci funkci main
    // tim konci i program...
    return 0;

}
