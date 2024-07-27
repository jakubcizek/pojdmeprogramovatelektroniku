/*

ZATIM TO NENI DOKONALE, CIRY EXPERIMENT !!
PRO SKENU NEMUSI ODHALIT NAPOPRVE VSE - DRUHA STRANA V TIMEOUTU NEODPOVI !!

Sestaveni v x64 vývojářské konzoli pro Visual C++ (VS Build Tools)

cl.exe /O1 /I Include skener.c /Fobuild/20240726_192650/ /Fe:build/20240726_192650/skener.exe /link /LIBPATH:Lib/x64 ws2_32.lib wpcap.lib

Anebo prilozeny powershellovy skript: make.ps1

./make.ps1 - Prelozi program a archivuje sestaveni do ./build/YYYYMMDD_HHMMSS/
./make.ps1 clean - Smaze archivy a spustitelne programy

Prerekvizity:

V adresari ./Include museji byt hlavickove soubory knihovny baliku Npcap
V adresari ./Lib musi byt knihovny baliku Npcap

!!! ZAROVEN JE TREBA NAINSTALOVAT SAMOTNY NPCAP !!!

Samotny Npcap a ZIP balicek npcap-sdk s SDK adresari stahnete z https://npcap.com/#download

*/


#include <pcap.h> // Knihovna paketoveho zachytavace Npcap pro Windows (https://npcap.com/)
#include <windows.h> // Jedna se o C program pro Windows
#include <stdbool.h> // Typ bool

// Hodnoty pro nastaveni ethernetoveho framu s ARP dotazem
// Viz https://en.wikipedia.org/wiki/Address_Resolution_Protocol#Packet_structure
// A jeste> https://en.wikipedia.org/wiki/EtherType
#define ETHERNET_TYPE_ARP 0x0806 // Ethernetovy frame typu ARP
#define ARP_REQUEST 1 // ARP dotaz
#define ARP_REPLY 2 // ARP odpoved
#define ARP_HTYPE_ETHERNET 1 // Ethernet jako linkovy protokol
#define ARP_PTYPE_IPV4 0x0800 // IPv4 jako IP protokol
#define ARP_HLEN_MAC 6 // Delka MAC
#define ARP_PLEN_IPV4 4 // Delka IPv4
#define ARP_FRAME_LEN 42 // Velikost framu

// Pomocna makra
#define TIMEOUT_MS 15 // Timeout pro cekani na ARP odpoved; kratsi hodnota zrychli sken vsech IP, ale zase nemusi dorazirt odpoved
#define RETRIES 2 // ARP frame muzeme odeslat vicekrat, a zvysit tak pravdepodobnost odpovedi,a le za cenu delsi doby
#define SLEEP_MS 5 // Drobna prodleva, ktera v programu zvysuje pravdepodobnost, ze maji vsechny operace dostatek casu
#define MAX_DEVICES 254 // Maximalni pocet skenovanych IP adres - predpokladame, ze subnet ma masku pro posledni blok IPv4 (255.255.255.0)

// Makra pro obarveni textu pomoci RGB
// Makra predstavuji obalku textu, ktery se ma zbarvit
// Takze text "Ahoj, \x1b[38;2;255;0;0mjak\x1b[0m se mas?"
// se v modernich terminalech zobrazi jako Ahoj, <cervene>jak</cervene> se mas?
#define COLOR_BEGIN(R, G, B) "\x1b[38;2;" #R ";" #G ";" #B "m"
#define COLOR_END "\x1b[0m"

#pragma pack(push, 1)

// Struktura ethernetove hlavicky
struct ethernet_frame_header {
    uint8_t dest[6]; // MAC prijemce (budeme v praxi posilat broadcastem = na vsechny)
    uint8_t src[6]; // MAC odesilatele
    uint16_t type; // Typ framu
};

// Struktura zpravy ARP (Address Resolution Protocol)
// Viz https://en.wikipedia.org/wiki/Address_Resolution_Protocol#Packet_structure
struct arp_message_header {
    uint16_t htype; // hardware type - linkovy protokol (ethernet = 1)
    uint16_t ptype; // internet protocol type - verze IP (IPv4 = 0x800)
    uint8_t hlen; // Delka hardwarove adresy (MAC = 6 B)
    uint8_t plen; // Delka protokolove adresy (IPv4 = 4 B)
    uint16_t oper; // Typ operace (dotaz = 1, odpoved = 2 )
    uint8_t sha[6]; // Hardwarova adresa odesilatele (MAC)
    uint8_t spa[4]; // Protokolova adresa odesilatele (IPv4)
    uint8_t tha[6]; // Hardwarova adresa prijemce (MAC) - TOTO BUDEME ZJISTOVAT A DOPREDU TO NEZNAME
    uint8_t tpa[4]; // Protokolova adresa prijemce (IPv4)
};

// Struktura ethernetoveho framu pro ARP zpravu
struct arp_frame {
    struct ethernet_frame_header ethernet; // Ethernetova hlavicka
    struct arp_message_header arp; // ARP zprava
};


// Struktura pro zaznam OUI
typedef struct {
    uint32_t oui; // OUI, prvnich 24 bitu z MAC
    char name[256]; // Odpovidajici textovy popisek vyrobce` SNAD BUDE 256 B STACIT
} OUI;

// Ukazatel na pamet s OUI identifikatory
// Nacteme je ze souboru oui.txt, 
// a protoze jich jsou desitky tisc,
// vytvorime je az za behu v dynamicke pameti
OUI *ouis = NULL;
size_t ouis_count = 0;

// Struktura pro nalezene zarizeni v siti
struct device{
    uint8_t ip[4]; // IPv4
    uint8_t mac[6]; // MAC
};

// Pole s nalezenymi zarizenimi v siti
// Pole ma pro jendoduchost pevnou velikost 254 * (4 + 6) bajtu
struct device devices[MAX_DEVICES];
int devices_count = 0;
#pragma pack(pop)

// Deklarace funkci 
bool load_ouis(const char *ouis_filename, OUI **ouis, size_t *count); // Funkce pro nacteni OUI databaze oui.txt
uint32_t extract_oui_from_mac(const uint8_t *mac); // Funkce pro extrakci prvnich 24 bitu z mAC adresy - OUI identifikator vyrobce
const char* find_vendor_by_oui(uint32_t oui); // Funkce pro nalezeni vyrobce podle OUI
bool is_device_saved(const uint8_t *ip); // Funkce pro kontrolu, jestli tuto IP/zarizeni uz mame ulozenou
void save_device(const uint8_t *ip, const char *mac); // Funcke pro ulzoeni zarizeni -- tedy jeho IP a MAC
void get_my_ip_and_mac(pcap_if_t *device, pcap_t *capture, uint8_t *my_ip, uint8_t *my_mac); // Funkce pro zjisteni moji IP a MAC podle vybraneho sitoveho adapteru
void analyze_capture(uint8_t *request, const struct pcap_pkthdr *response_header, const uint8_t *response); // Funkce pro analyzu zachycenych paketu
void print_devices(); // Funkce pro vypsani vysledku -- nalezenych zarizeni v siti
int compare_devices(const void *a, const void *b); // Pomocna funkce pro razen9 zarizeni podle IP adresy


// Hlavni funkce -- zacatek programu
int main() {
    pcap_if_t *adapters;
    pcap_if_t *adapter;
    int adapter_number;
    pcap_t *capture;
    char err[PCAP_ERRBUF_SIZE];
    struct arp_frame frame;
    struct pcap_pkthdr *header;
    const uint8_t *reply;
    int res;
    int i = 0;
    uint8_t my_ip[4], my_mac[6];

    // Nacteme databazi OUI ze souboru oui.txt
    // OUI je prvnich 24 bitu z MAC adresy a identifiuje vyrobce sitoveho zarizeni
    // Soubor ma format XXXXXX VYROBCE
    printf("\n   *** Zive.cz skener site ***\n\nNacitam databazi OUI... ");
    if (load_ouis("oui.txt", &ouis, &ouis_count)) {
        printf("%zd zaznamu\n\n", ouis_count);
    }else{
        printf("Nelze otevrit oui.txt");
    }
    

    // Pomoci knihovny Npcap vyhledame vsechny sitove adaptery v PC
    if (pcap_findalldevs(&adapters, err) == -1) {
        printf("Chyba pri hledani zarizeni: %s\n", err);
        return 1;
    }

    // Vypiseme nazvy adapteru
    printf("Dostupna sitova zarizeni:\n");
    for (adapter = adapters; adapter != NULL; adapter = adapter->next) {
        if (strlen(adapter->description)>0) {
            printf("%d. "COLOR_BEGIN(50,255,50)"%s"COLOR_END"\n",++i, adapter->description);
        } else {
            printf("%d. "COLOR_BEGIN(50,255,50)"%s"COLOR_END"\n",++i, adapter->name);
        }
    }

    // Pokud nedokazeme identifikovat zadny adapter, ukoncime program
    // Mame enainstalovany nastroj Npcap (npcap.com)?
    if (i == 0) {
        printf("Nebyla nalezena zadna zarizeni!\nUjisti se, ze je nainstalovany Npcap (npcap.com).\n");
        return 1;
    }

    // Pozadame uzivatele o cislo adapteru ze seznamu
    printf("\nZadejte cislo zarizeni, ktere chcete pouzit: ");
    scanf("%d", &adapter_number);
    if (adapter_number < 1 || adapter_number > i) {
        printf("% je neplatne cislo zarizeni.\n", adapter_number);
        return 1;
    }

    // Vybereme adapter a otevreme jej v Npcapu pro zivy zachyt paketu
    for (adapter = adapters, i = 1; adapter != NULL && i != adapter_number; adapter = adapter->next, i++);
    if ((capture = pcap_open_live(adapter->name, BUFSIZ, 1, TIMEOUT_MS, err)) == NULL) {
        printf("Nelze otevrit zarizeni %s: %s\n", adapter->name, err);
        return 1;
    }

    // Ziskame IP a MAC adapteru
    // A rovnou je ulozime do databaze nalezenych zarizeni v siti - sami sebe vidime vzdy
    get_my_ip_and_mac(adapter, capture, my_ip, my_mac);
    save_device(my_ip, my_mac);

    // Ted nastavime ethernetovy frame, ktery budeme posilat do site
    // Nejprve nastavime jeho ethernetovou hlavicku
    memset(&frame, 0, sizeof(struct arp_frame));
    memset(frame.ethernet.dest, 0xFF, 6); // Frame posleme na broadcastovaci MAC adresu FF:FF:FF:FF:FF:FF, takze by mel dorazit vsem v subnetu
    memcpy(frame.ethernet.src, my_mac, 6); // Podepiseem se vlastni MAC adresou
    frame.ethernet.type = htons(ETHERNET_TYPE_ARP); // Nastavime typ ethernetoveho framu na ARP

    // Jeste doplnime cast s ARP parametry
    // V podstate vytvarime zpravu: "Ahoj, ja jsem XYZ! kdo mate IP adresu ABC, napiste mi svoji MAC"
    frame.arp.htype = htons(ARP_HTYPE_ETHERNET); // Pouzivame ethernet
    frame.arp.ptype = htons(ARP_PTYPE_IPV4); // Pouzivame IPv4
    frame.arp.hlen = ARP_HLEN_MAC; // Delka MAC
    frame.arp.plen = ARP_PLEN_IPV4; // Delka IPv4
    frame.arp.oper = htons(ARP_REQUEST); // Jedna se o ARP dotaz
    memcpy(frame.arp.sha, my_mac, 6); // Moje MAC
    memcpy(frame.arp.spa, my_ip, 4); // Moje IPv4
    memset(frame.arp.tha, 0, 6); // MAC prijemce nezname, a proto vyplnime nulou

    // Zjisteni rozsahu podle sitoveho adpteru, ktery budeme skenovat (treba 192.168.1.1-192.168.1.254 atp.)
    uint32_t net_ip, net_mask;
    if (pcap_lookupnet(adapter->name, &net_ip, &net_mask, err) == -1) {
        printf("Chyba pri zjistovani sitove masky pro zarizeni %s: %s\n", adapter->name, err);
        return 1;
    }
    uint32_t ip = ntohl(net_ip);
    uint32_t mask = ntohl(net_mask);
    uint32_t network = ip & mask;
    uint32_t broadcast = network | ~mask;

    i = 0; // Resetujeme pocitadlo, ktere ted bude slouzit pro vypis, kolik IP adres jsme proskenovali

    // Projdeme rozsah IP adres
    for (uint32_t target_ip = network + 1; target_ip < broadcast; ++target_ip) {
        // Do ethernetoveho framu, ktery jsme si vytvorili vyse,
        // vlozime IP adresu, jejiz MAC chceme zrovna zjistit
        *(uint32_t *)frame.arp.tpa = htonl(target_ip);

        printf("Skenovano adres: %d\n", ++i);
        printf("Zachyceno odpovedi ARP: %d\n", devices_count);
        printf("\033[2A");

        // Ne kazde zarizeni na ARP zareaguje hned, a tak muzeme poslat hned nekolik framu po sobe
        // a zvysit tak sance na odpoved (ale pozor na nevhodnou komunikaci a zahlceni site)
        for (int attempt = 0; attempt < RETRIES; ++attempt) {
            if (pcap_sendpacket(capture, (const uint8_t *)&frame, ARP_FRAME_LEN) != 0) {
                printf("\033[2B");
                fprintf(stderr, "Chyba pri odesilani framu: %s\n", pcap_geterr(capture));
                return 1;
            }
            Sleep(SLEEP_MS); // Drobna pauza
            // Precteme v timeoutu zachycene pakety a zpracujeme je ve funkci analyze_capture
            pcap_dispatch(capture, -1, analyze_capture, (uint8_t *)&frame);  
        }
    }
    printf("\033[2B\n"); // Ukonceni statickeho bloku s vypisem zive statistiky a odradkovani

    Sleep(TIMEOUT_MS * 2); // Drobna pauza

    // Seradime nalezena zarizeni podle IP (odpovedi nemusely dojit ve spravnem poradi)
    // A vypiseme vysledky do terminalu
    qsort(devices, devices_count, sizeof(struct device), compare_devices);
    print_devices();

    // uvolnime prostredky Npcapu
    // Uvolnime dynamickou pamet s databazi OUI
    pcap_close(capture);
    pcap_freealldevs(adapters);
    free(ouis);

    // Koncime program standardnim navratovym kodem
    return 0;
}

bool load_ouis(const char *ouis_filename, OUI **ouis, size_t *count) {
    FILE *file = fopen(ouis_filename, "r");
    if (!file) {
        return false;
    }
    char line[256];
    size_t _count = 0;
    size_t capacity = 10;
    *ouis = (OUI *)malloc(capacity * sizeof(OUI));
    while (fgets(line, sizeof(line), file)) {
        if (_count >= capacity) {
            capacity *= 2;
            *ouis = (OUI *)realloc(*ouis, capacity * sizeof(OUI));
        }
        char *token = strtok(line, " ");
        if (token) {
            sscanf(token, "%x", &(*ouis)[_count].oui);
            token = strtok(NULL, "\n");
            if (token) {
                strncpy((*ouis)[_count].name, token, sizeof((*ouis)[_count].name) - 1);
                (*ouis)[_count].name[sizeof((*ouis)[_count].name) - 1] = '\0';
                _count++;
            }
        }
    }

    fclose(file);
    *count = _count;
    return true;
}

// Funkce pro vytazeni prvnich 24 bitu z MAC
uint32_t extract_oui_from_mac(const uint8_t *mac) {
    uint32_t oui = 0;
    oui |= ((uint32_t)mac[0] << 16);
    oui |= ((uint32_t)mac[1] << 8);
    oui |= (uint32_t)mac[2];
    return oui;
}

// Funkce pro vyhledání textového řetězce podle 24-bitového identifikátoru
const char* find_vendor_by_oui(uint32_t oui) {
    for (size_t i = 0; i < ouis_count; i++) {
        if (ouis[i].oui == oui) {
            return ouis[i].name;
        }
    }
    return "*";
}

bool is_device_saved(const uint8_t *ip) {
    for (int i = 0; i < devices_count; ++i) {
        if (memcmp(devices[i].ip, ip, 4) == 0) {
            return true;
        }
    }
    return false;
}

void save_device(const uint8_t *ip, const char *mac) {
    if (devices_count < MAX_DEVICES && !is_device_saved(ip)) {
        memcpy(devices[devices_count].ip, ip, 4);
        memcpy(devices[devices_count].mac, mac, 6);
        ++devices_count;
    }
}

void get_my_ip_and_mac(pcap_if_t *device, pcap_t *capture, uint8_t *my_ip, uint8_t *my_mac) {
    struct pcap_pkthdr *header;
    const uint8_t *packet;
    int res;
    while ((res = pcap_next_ex(capture, &header, &packet)) >= 0) {
        if (res == 0) continue;

        struct ethernet_frame_header *eth = (struct ethernet_frame_header *)packet;
        if (ntohs(eth->type) == 0x0800) {  
            memcpy(my_mac, eth->src, 6);
            struct in_addr addr;
            memcpy(&addr, packet + 26, sizeof(struct in_addr));
            memcpy(my_ip, &addr, 4);
            break;
        }
    }
    if (res == -1) {
        printf("Chyba pri cteni paketu: %s\n", pcap_geterr(capture));
        exit(1);
    }
}

// Funkce pro analyzu paketu zachycenych Npcapem
void analyze_capture(uint8_t *request, const struct pcap_pkthdr *response_header, const uint8_t *response) {
    // ARP frame, ktery jsme poslali do site
    struct arp_frame *arp_request = (struct arp_frame *)request;
    // Ethernetova hlavicka odpovedi 
    struct ethernet_frame_header *eth = (struct ethernet_frame_header *)response;
    // Zkontrolujeme, jestli se jedna o odpoved protokolu ARP 
    if (ntohs(eth->type) == ETHERNET_TYPE_ARP) {
        // Pointer na ARP zpravu v odpovedi
        struct arp_message_header *arp = (struct arp_message_header *)(response + sizeof(struct ethernet_frame_header));
        // Zkontrolujeme, jestli se jedna o zpravu typu ARP odpovedi
        // A zaroven, jestli se jedna o odpoved ze zarizeni, ktera ma stejnou IP adresu, na kterou jsme se zrovna ptali
        if (ntohs(arp->oper) == ARP_REPLY && memcmp(arp->tpa, arp_request->arp.spa, 4) == 0) {
            // Do seznamu zachycenych zarizeni pridame novou IP a jeji MAC
            // ale jen v pripade, ze tam uz není
            if (!is_device_saved(arp->spa)) {
                save_device(arp->spa, arp->sha);
            }
        }
    }
}

// vypiseme tabulku zarizení, ktere odpovedely na protokolu ARP
// Tabulka je serazena pdole IP a obsahuej take MAC a podle OUI identifikovaneho vyrobce
void print_devices() {
    printf("%-*s%-*s%-*s\n", 17, "IP adresa", 19, "MAC adresa", 53, "Vyrobce");
    printf("------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < devices_count; i++) {
        // Vytvorime textovy retezec s IP adresou
        char ip_str[16];
        sprintf(ip_str, "%d.%d.%d.%d",
            devices[i].ip[0], devices[i].ip[1], devices[i].ip[2], devices[i].ip[3]);
        // Vytvorime textovy retezec s MAC adresou
        char mac_str[18];
        sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            devices[i].mac[0], devices[i].mac[1], devices[i].mac[2], devices[i].mac[3], devices[i].mac[4], devices[i].mac[5]);
        // Vytahneme z MAC prvnich 24 bitu – oui
        uint32_t oui = extract_oui_from_mac(devices[i].mac);
        // Vyhledame oui v databazi vyrobcu oui.txt, ktera je uz cela nactena v RAM
        const char *vendor = find_vendor_by_oui(oui);
        // Vypiseme radek s vysledky a vyrobce zbarvime do zelena (RGB 50,255,50)
        printf("%-*s%-*s"COLOR_BEGIN(50,255,50)"%-*s"COLOR_END"\n", 17, ip_str, 19, mac_str, 53, vendor);

    }
}

// Porovnavvaci funkce pro qsort, ktera porovna dve struktury typu device pdoe jejich ip adres (4 bajtu)
int compare_devices(const void *a, const void *b) {
    struct device *deviceA = (struct device *)a;
    struct device *deviceB = (struct device *)b;
    // Porovnání jednotlivých bajtů IP adres
    for (int i = 0; i < 4; i++) {
        if (deviceA->ip[i] < deviceB->ip[i]) {
            return -1;
        } else if (deviceA->ip[i] > deviceB->ip[i]) {
            return 1;
        }
    }
    return 0;
}
