#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
    #include <windows.h>
    #define smazatObrazovku system("cls")
    #define pockejMs(ms) (Sleep(ms))
#else
    #include <unistd.h>
    #define smazatObrazovku system("clear")
    #define pockejMs(ms) (usleep(ms*1000))
#endif

#include "animace.h"

#define MAX_POKUSY 3

int main(){
    char heslo[20];
    int volba = 0;
    uint8_t prihlasen = 0;
    uint8_t pokusy = 0;
    uint8_t smycka = 1;

    smazatObrazovku;

    puts(
        "                                                          \r\n"
        "          ) ) )                     ) ) )                 \r\n"
        "        ( ( (                      ( ( (                  \r\n"
        "      ) ) )                       ) ) )                   \r\n"
        "   (~~~~~~~~~)                 (~~~~~~~~~)                \r\n"
        "    | havl- |                   | brod  |                 \r\n"
        "    | ickuv |                   |       |                 \r\n"
        "    I      _._                  I       _._               \r\n"
        "    I    /'   `\\                I    /'   `\\           \r\n"
        "    I   |       |               I    |      |             \r\n"
        "    f   |   |~~~~~~~~~~~~~~|    f    |    |~~~~~~~~~~~~~~|\r\n"
        "  .'    |   ||~~~~~~~~|    |  .'     |    | |~~~~~~~~|   |\r\n"
        "/'______|___||__###___|____|/'_______|____|_|__###___|___|\r\n"
        "                                                          \r\n"
        "                                                          \r\n"
        "            JADERNA ELEKTRARNA HAVLICKUV BROD             \r\n"
        "                     Ovladaci pult 1.0                    \r\n"
        "                                                          \r\n"
        "              Statni ustav cislicovy,  s.p.               \r\n"
        "                                                          \r\n"
    );

    while(prihlasen == 0){

        puts("");

        if(pokusy == MAX_POKUSY){
            puts(
                "Zjevne neznate spravne heslo!\r\n"
                "Ukoncuji program a predavam vase udaje NUKIB!"
            );
            goto KONEC;
        }

        printf("Zadejte heslo pana reditele: ");
        gets(heslo);

        if(strcmp(heslo, "papousek") != 0){
            printf("%s%d. spatny pokus, zkuste to znovu!%s\r\n", CERVENA, (pokusy+1), ZADNA);
            pokusy++;
        }
        else{
            prihlasen = 1;
        }
    }

    #ifdef KONTROLA
        puts("");
        puts("---------------------------------------------------------");
        puts("Kontrolni vypis stavu promennych programu (vychozi stav):");
        printf("Heslo: %s\r\nVolba: %d (0)\r\nPrihlasen: %d (0)\r\nPokusy: %d (0)\r\nSmycka: %d (0)\r\n",
            heslo, volba, prihlasen, pokusy, smycka);
        puts("---------------------------------------------------------");
        puts("");
    #endif
    
    puts(
        "\r\n"
        ZELENA
        "Vita vas ovladaci panel jaderne elektrarny\r\n\r\n"
        ZADNA
        "Pro provedeni akce stisknete:\r\n"
        "\t1 ... pro pretizeni reaktoru a zniceni Ceske republiky\r\n"
        "\t2 ... pro olajkovani poradu Tyden Zive na YouTube\r\n"
        "\t3 ... pro ukonceni programu"
    );
    while(smycka){
        printf("\r\nCo si prejete provest: ");
        scanf("%d", &volba);
        switch(volba){
            case 1:
                puts(
                    "\r\n"
                    "Spoustim instrukci 0x65AC0209 pro pretizeni reaktoru Zuzana-I3B\r\n"
                    "Prosim, sporadane se odeberte do podzemnich krytu, dekuji!\r\n"
                );
                for(uint8_t i=10;i>0;i--){
                    printf("%d...\r\n", i);
                    pockejMs(500);
                }
                smazatObrazovku;
                animujVybuch();
                smycka = 0;
            break;
            case 2:
                puts(
                    "\r\n"
                    "Spoustim instrukci 0xAABBCC pro olajkovani poradu Tyden Zive na YouTube\r\n"
                    "Rizenym DDoS utokem skrze botnet Emotet a zavirovane pocitace na\r\n"
                    "personalnim oddeleni olajkovano 1 459 poradu"
                );
            break;
            case 3:
                puts("TAk zase na videnou!");
                smycka = 0;
            break;
            default:
                printf(
                    "Instrukci (%d) neschvalilo Ministerstvo prumyslu CR!\r\n"
                    "Zvolte jinou dle normy CSN ETS 300 738 ED.1\r\n", volba);
        }
    }

KONEC:

    puts("");
    printf("Ukoncuji spojeni se statnim cloudem... ");
    pockejMs(2000);
    printf("OK\r\n");
    printf("Mazu citliva data... ");
    pockejMs(2000);
    printf("OK\r\n");
    printf("Formatuji systemovy oddil... ");
    pockejMs(2000);
    printf("OK\r\n");
    printf("Mazu obsah displeje... ");
    pockejMs(2000);
    printf("OK\r\n");
    pockejMs(2000);
    smazatObrazovku;

    return 0;
}