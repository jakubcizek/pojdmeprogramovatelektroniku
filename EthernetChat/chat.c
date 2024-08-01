
// Prekladac:
// Predpokladame uspesne nainstalovani Build Tools for Visual Studio 2022 (https://visualstudio.microsoft.com/cs/downloads/#build-tools-for-visual-studio-2019) a v nem SDK pro preklad C/C++ na Windows
// Prekladac spoustime v 64bit vyvojarskem PoweShellu
//
// Povel k prekladu:
// cl.exe chat.c /I Include /link /LIBPATH:Lib/x64 wpcap.lib pdcurses.lib user32.lib advapi32.lib ws2_32.lib

#include <pcap.h> // Knihovna paketoveho zachytavace Npcap pro Windows (https://npcap.com/ -- je treba nainstalovat i samotny ovladac do Windows)
#include <curses.h> // Textove UI, knihovna PDCurses (https://pdcurses.org/)
#include <windows.h> // Hlavni hlavicka pri programovani pro Windows, zapouzdruje vetsinu obvyklych hlavicek
#include <time.h> // Prace s casem

// EtherType ID naseho chatu
// Viz https://en.wikipedia.org/wiki/Ethernet_frame#Ethernet_II
// Viz https://en.wikipedia.org/wiki/EtherType
// Seznam zabranych cisel EtherType: https://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.xhtml
#define ETHERTYPE_CHAT 0xFFAA

// Timeout pro poslech sitove aktivity v ms
#define TIMEOUT_MS 100

// Makra pro obarveni textu v terminalu pomoci RGB
// Makra predstavuji obalku textu, ktery se ma zbarvit
// Takze text "Ahoj, \x1b[38;2;255;0;0mjak\x1b[0m se mas?"
// se v modernich terminalech zobrazi jako Ahoj, <cervene>jak</cervene> se mas?
#define BARVA_ZACATEK(R, G, B) "\x1b[38;2;" #R ";" #G ";" #B "m"
#define BARVA_KONEC "\x1b[0m"

// Pomocna makra pro konstrukci okna chatu
#define ZPRAVA_DELKA 200
#define VSTUPNI_PANEL_VYSKA 3
#define ZPRAVY_PAMET 100

#pragma pack(push, 1)
// Struktura ethernetove hlavicky
// 14 bajtu
struct ethernet_hlavicka {
    uint8_t mac_prijemce[6]; // MAC prijemce (budeme v praxi posilat broadcastem = na vsechny)
    uint8_t mac_odesilatel[6]; // MAC odesilatele
    uint16_t ethertype; // Typ framu
};

// Telo s ramce s protokolem naseho chatu
// 250 bajtu
struct ethernet_telo {
    char prijemce[25];
    char odesilatel[25];
    char zprava[ZPRAVA_DELKA];
};

// Struktura celeho ethernetoveho framu s chatovou zpravou
// 14 + 250 = 264 bajtu
#define RAMEC_VELIKOST 264
struct ethernet_ramec {
    struct ethernet_hlavicka ethernet; // Ethernetova hlavicka
    struct ethernet_telo telo; // Chatovy protokol
};

// Struktura chatove UI zpravy v okne
struct ui_zprava{
    char text[ZPRAVA_DELKA + 10]; //  Zprava + casova znacka
    int barva; // 1 = zelena, 2 = modra
};
#pragma pack(pop)

pcap_t *pcap; // Objekt zachytavace sitove komunikace
// Pamet na prezdivku, nejvyse 24 znaku (+ ukoncovaci znak)
// Je mozne zmenit pri startu parametrem --prezdivka text
char prezdivka[25] = "Anonym";
WINDOW *vystup, *vstup; // Vystupni (seznam zprav) a vstupni (zadavani zpravy) UI cast
char zprava[ZPRAVA_DELKA]; // PAmet na odesilanou zpravu
struct ui_zprava ui_zpravy[ZPRAVY_PAMET]; // Pole UI zprav
int zpravy_pocet = 0; // Pocet zprav
int aktualni_index_zpravy = 0; // Aktualni index/pozice zpravy
CRITICAL_SECTION cs; // Zamek/semafor pro vicevlaknove operace

// Uizvatelske funkce
void ziskat_moji_mac(pcap_if_t *adapter, pcap_t *pcap, uint8_t *moje_mac); // Ziskej MAC adresu zvoleneho sitoveho adapteru
void analyzuj_ramec(uint8_t *dotaz, const struct pcap_pkthdr *odpoved_hlavicka, const uint8_t *odpoved); // Analyzuj ethernetovy ramec
void aktualni_cas(char *txt, size_t delka); // Ziskej aktualni cas a uloz ho jako text ve formatu HH:MM:SS
DWORD WINAPI vlakno_zachytavani_ramcu(LPVOID argumenty); // Paraleleni vlakno pro zachytavani sitove komunikace

// Funkce textoveho UI - prace s knihovnou PDCurses
void inicializace_textoveho_okna(WINDOW **vystup, WINDOW **vstup); // Vytvoreni textoveho UI
void smazat_okna(WINDOW *vystup, WINDOW *vstup); // Smazani textoveho UI
void vypsat_zpravy(); // Vypis pameti zprav
void ziskat_vstup(WINDOW *okno, char *txt, int delka); // Cteni textoveho vstupu

// HLAVNI FUNKCE -- ZACATEK PROGRAMU
int main(int argumenty_pocet, char *argumenty[]) {
    // Identifikatory vlakna, ktere bude cekat na ramce s chatovymi odpovedmi
    HANDLE vlakno;
    DWORD vlakno_id;
    pcap_if_t *adaptery; // Sitove adaptery v PC
    pcap_if_t *adapter; // Vybrany sitovy adapter
    int adapter_cislo; // Cislo adapteru v poradi
    char chyba[PCAP_ERRBUF_SIZE]; // Pamet na chybu
    struct ethernet_ramec ramec; // Ramec na chatovou pzravu
    int pocitadlo = 0; // Obecne pocitadlo pro ruzne smycky
    uint8_t moje_mac[6]; // PAmet na MAC adresu adapteru

    // Pokud jsme spustili program s argumentem --prezdivka XXX,
    // XXX ulozime jako novou prezdivku v chatu
    for (pocitadlo = 1; pocitadlo < argumenty_pocet; pocitadlo++) {
        if (strcmp(argumenty[pocitadlo], "--prezdivka") == 0 && pocitadlo + 1 < argumenty_pocet) {
            strcpy(prezdivka, argumenty[pocitadlo + 1]);
            pocitadlo++;
        }
    }

    // Pomoci knihovny Npcap vyhledame vsechny sitove adaptery v PC
    if (pcap_findalldevs(&adaptery, chyba) == -1) {
        printf("Chyba pri hledani zarizeni: %s\n", chyba);
        return 1;
    }

    // Vypiseme nazvy adapteru
    printf("Dostupna sitova zarizeni:\n");
    pocitadlo = 0;
    for (adapter = adaptery; adapter != NULL; adapter = adapter->next) {
        if (strlen(adapter->description)>0) {
            printf("%d. "BARVA_ZACATEK(50,255,50)"%s"BARVA_KONEC"\n",++pocitadlo, adapter->description);
        } else {
            printf("%d. "BARVA_ZACATEK(50,255,50)"%s"BARVA_KONEC"\n",++pocitadlo, adapter->name);
        }
    }

    // Pokud nedokazeme identifikovat zadny adapter, ukoncime program
    // Mame enainstalovany nastroj Npcap (npcap.com)?
    if (pocitadlo == 0) {
        printf("Nebyla nalezena zadna zarizeni!\nUjisti se, ze je nainstalovany Npcap (npcap.com).\n");
        return 1;
    }

    // Pozadame uzivatele o cislo adapteru ze seznamu
    printf("\nZadejte cislo zarizeni, ktere chcete pouzit: ");
    scanf("%d", &adapter_cislo);
    if (adapter_cislo < 1 || adapter_cislo > pocitadlo) {
        printf("%d je neplatne cislo zarizeni.\n", adapter_cislo);
        return 1;
    }

    // Vybereme adapter a otevreme jej v Npcapu pro zivy zachyt paketu
    for (adapter = adaptery, pocitadlo = 1; adapter != NULL && pocitadlo != adapter_cislo; adapter = adapter->next, pocitadlo++);
    if ((pcap = pcap_open_live(adapter->name, BUFSIZ, 1, TIMEOUT_MS, chyba)) == NULL) {
        printf("Nelze otevrit zarizeni %s: %s\n", adapter->name, chyba);
        return 1;
    }

    printf("Zjistuji MAC...");
    // Ziskame MAC adapteru, se kterym pracujeme
    ziskat_moji_mac(adapter, pcap, moje_mac);
    printf("OK\n");

    // Sestavime ethernetovy ramec pro chat, ale zatim nevyplnime text zpravy
    memset(&ramec, 0, sizeof(struct ethernet_ramec));
    memset(ramec.ethernet.mac_prijemce, 0xFF, 6); // Frame posleme na broadcastovaci MAC adresu FF:FF:FF:FF:FF:FF, takze by mel dorazit vsem v subnetu
    memcpy(ramec.ethernet.mac_odesilatel, moje_mac, 6); // Podepiseem se vlastni MAC adresou
    ramec.ethernet.ethertype = htons(ETHERTYPE_CHAT); // Nastavime typ ethernetoveho framu na nas chat
    strcpy(ramec.telo.odesilatel, prezdivka);
    strcpy(ramec.telo.prijemce, "vsichni"); // Nas chat zatim nema cilene adresovani treba zvyraznenim textu atp., a tak jako prijemce nastavime "vsichni"

    // Vytvorime textove UI chatu pomoci knihovny PDCurses
    inicializace_textoveho_okna(&vystup, &vstup);

    // K pameti se zpravami budeme pristupovat z vicero vlaken, a tak si pripravime zamek/semafor,
    // ktery vyhradi pristup vzdy jen pro jendoho hrace, aby nedoslo ke kolizi
    InitializeCriticalSection(&cs); 

    // Nastartujeme vlakno, ktere kontroluje, jestli nedorazily ethernetove ramce s chatovymi odpovedmi
    vlakno = CreateThread(NULL, 0, vlakno_zachytavani_ramcu, (LPVOID)&ramec, 0, &vlakno_id); 
    if (vlakno == NULL) {
        printf("Chyba pri vytvareni vlakna\n");
        pcap_freealldevs(adaptery);
        return 1;
    }

    // Nekonecna smycka, ve ktere kontrolujeme textovy vstup
    // a posilame jej jako ethernetove ramce chatu do site
    while (1) {
        wrefresh(vstup);
        // Ziskat text z UI panelu u paticky
        ziskat_vstup(vstup, zprava, ZPRAVA_DELKA);
	EnterCriticalSection(&cs); // Vicevlaknovy zamek
        // Pokud jsme napsali konec, program skonci
        if (strcmp(zprava, "konec") == 0) {
            break;
        }
        // Zkopirujeme text do predpripraveneho ramce
	strcpy(ramec.telo.zprava, zprava);
        // Odesleme ramec do site
        pcap_sendpacket(pcap, (const uint8_t *)&ramec, RAMEC_VELIKOST);
        LeaveCriticalSection(&cs); // Uvolneni vicevlaknoveho zmaku
    }

    // Ukoncime vlakna a uvolnime prostredky
    TerminateThread(vlakno, 0);
    CloseHandle(vlakno);
    DeleteCriticalSection(&cs);
    // Smazeme textove UI
    smazat_okna(vystup, vstup);
    // Ukoncime program s normalnim navratovym kodem
    return 0;
}

DWORD WINAPI vlakno_zachytavani_ramcu(LPVOID argumenty) {
    pcap_loop(pcap, -1, analyzuj_ramec, (uint8_t *)argumenty);
    return 0;
}

void ziskat_moji_mac(pcap_if_t *adapter, pcap_t *pcap, uint8_t *moje_mac) {
    struct pcap_pkthdr *hlavicka;
    const uint8_t *ramec;
    int odpoved;
    while ((odpoved = pcap_next_ex(pcap, &hlavicka, &ramec)) >= 0) {
        if (odpoved == 0) continue;
        struct ethernet_hlavicka *eth = (struct ethernet_hlavicka *)ramec;
        if (ntohs(eth->ethertype) == 0x0800) {  
            memcpy(moje_mac, eth->mac_odesilatel, 6);
            break;
        }
    }
    if (odpoved == -1) {
        printf("Chyba pri cteni ramce: %s\n", pcap_geterr(pcap));
        exit(1);
    }
}

void aktualni_cas(char *txt, size_t delka) {
    time_t ted = time(NULL);
    struct tm *cas = localtime(&ted);
    strftime(txt, delka, "%H:%M:%S", cas);
}


void inicializace_textoveho_okna(WINDOW **vystup, WINDOW **vstup) {
    initscr();
    cbreak();
    noecho();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_GREEN, -1);
    init_pair(2, COLOR_BLUE, -1);
    init_pair(3, COLOR_WHITE, -1);
    int rodic_x, rodic_y;
    getmaxyx(stdscr, rodic_y, rodic_x);
    *vystup = newwin(rodic_y - VSTUPNI_PANEL_VYSKA, rodic_x, 0, 0);
    *vstup = newwin(VSTUPNI_PANEL_VYSKA, rodic_x, rodic_y - VSTUPNI_PANEL_VYSKA, 0);
    scrollok(*vystup, TRUE);
    box(*vstup, 0, 0);
    wattron(*vstup, COLOR_PAIR(3));
    mvwprintw(*vstup, 1, 1, "%s: ", prezdivka); 
    wrefresh(*vystup);
    wrefresh(*vstup);
}

void smazat_okna(WINDOW *vystup, WINDOW *vstup) {
    delwin(vystup);
    delwin(vstup);
    endwin();
}

void vypsat_zpravy() {
    EnterCriticalSection(&cs);
    werase(vystup);
    int start = aktualni_index_zpravy - zpravy_pocet;
    if (start < 0) start += ZPRAVY_PAMET;
    for (int i = 0; i < zpravy_pocet; i++) {
        int idx = (start + i) % ZPRAVY_PAMET;
        wattron(vystup, COLOR_PAIR(ui_zpravy[idx].barva));
        wprintw(vystup, "%s\n", ui_zpravy[idx].text);
        wattroff(vystup, COLOR_PAIR(ui_zpravy[idx].barva));
    }
    wrefresh(vystup);
    wmove(vstup, 1, strlen(prezdivka) + 3); 
    wrefresh(vstup);
    LeaveCriticalSection(&cs);
}


void ziskat_vstup(WINDOW *okno, char *txt, int delka) {
    echo();
    int pozice_kurzor = strlen(prezdivka) + 3;
    mvwgetnstr(okno, 1, pozice_kurzor, txt, delka - 1);
    noecho();
    wclear(okno);
    box(okno, 0, 0);
    wattron(okno, COLOR_PAIR(3));
    mvwprintw(okno, 1, 1, "%s: ", prezdivka); 
    wrefresh(okno);
}


void analyzuj_ramec(uint8_t *dotaz, const struct pcap_pkthdr *odpoved_hlavicka, const uint8_t *odpoved) {
    // Ethernetova hlavicka odpovedi 
    struct ethernet_hlavicka *hlavicka = (struct ethernet_hlavicka *)odpoved;
    // Zkontrolujeme, jestli se jedna o ethernetovy ramec s ethertype naseho chatu 
    if (ntohs(hlavicka->ethertype) == ETHERTYPE_CHAT) {
        // Ukazatel na telo ethernetoveho ramce s nasi chatovou zpravou
        struct ethernet_telo *telo = (struct ethernet_telo *)(odpoved + sizeof(struct ethernet_hlavicka));
	EnterCriticalSection(&cs); // Pracujeme v paralelnim vlakne a budeme pracovat s globalnimi promennymi, takze je zamkneme pro sebe, aby nedoslo ke kolizi
	// Ziskame akualni cas jako text
        char cas[9];
        aktualni_cas(cas, sizeof(cas));
        // Vytvorime novou UI zpravu ve formatu [cas] odesilatel: zprava
	snprintf(ui_zpravy[aktualni_index_zpravy].text, sizeof(ui_zpravy[aktualni_index_zpravy].text), "[%s] %s: %s", cas, telo->odesilatel, telo->zprava);
        // Nastavime barvu zpravy na modrou (pozdeji kod muzeme vylepsit a zpravy barevne rozlisovat treba podle odesilatele, prijemce atp.)
        ui_zpravy[aktualni_index_zpravy].barva = 2;
        // Posuneme pamet na zpravy o jeden (nejstarsi zprava zmizi)
        aktualni_index_zpravy = (aktualni_index_zpravy + 1) % ZPRAVY_PAMET;
        if (zpravy_pocet < ZPRAVY_PAMET) {
            zpravy_pocet++;
        }
        vypsat_zpravy(); // Vypiseme zpravy na obrazovku
	LeaveCriticalSection(&cs); // Odemkneme zamek
    }
}
