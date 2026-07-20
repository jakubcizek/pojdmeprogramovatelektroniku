/*
 * Sportovní časomíra pro amatérské cyklistické závody.
 *
 * Snímač: optická závora E3F-R2N2, jejíž výstup je přes Schottkyho diodu
 * přiveden na GPIO. Cyklista přerušením paprsku spustí a následně (po slepé
 * době) zastaví měření. Přesný čas hrany se odečítá přímo v přerušení
 * z esp_timer (rozlišení 1 µs).
 *
 * Původně napsáno pro Arduino, přepsáno na čisté ESP-IDF.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gpio_filter.h"
#include "driver/uart.h"
#include "esp_rom_uart.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ble_uart.h"
#include "ssd1306.h"
#include "ota_wifi.h"
#include "proto.h"

// ================== Konfigurace ==================
// Hardware: ESP32-S3-DevKitC-1
#define SENSOR_PIN      GPIO_NUM_10        // výstup 12V optozávory E3F-R2N2 přes Schottkyho diodu (1N5817)
#define BUTTON_PIN      GPIO_NUM_11        // tlačítko pro ruční ovládání (spíná do GND)
#define CONSOLE_UART    UART_NUM_0         // konzole DevKitC-1 (115200 Bd, port "UART")
#define I2C_SDA_PIN     GPIO_NUM_8         // OLED SSD1306 - SDA
#define I2C_SCL_PIN     GPIO_NUM_9         // OLED SSD1306 - SCL
#define OLED_ADDR       0x3C               // 7bitová adresa (modul značený "0x78")
#define UART_ALIVE_MS   3000               // bez zprávy z UART déle = spojení mrtvé
#define NVS_NAMESPACE   "casomira"         // NVS namespace pro persistentní nastavení
#define NVS_KEY_BLIND   "blind_ms"         // klíč: slepá doba [ms]

static const char *TAG = "casomira";

// ============ Strojový protokol (UART 115200 Bd i BLE NUS) ============
// Stejné řádky tečou po USB UART i přes Bluetooth (Nordic UART Service).
// Deska -> web, vždy s prefixem "CSM " a ukončené '\n':
//   CSM HELLO proto=1 blind_ms=<ms>   po startu desky, potvrzení linky
//   CSM READY                          časomíra vyzbrojena, čeká na paprsek
//   CSM IDLE                           časomíra zamčena (boot / změna režimu
//                                      připojení) - čeká na nové povolení
//   CSM START us=<t>                   1. přerušení paprsku = start kola
//   CSM RESULT us=<dur>                změřené trvání kola v mikrosekundách
//   CSM AUTH ok|fail|required          odpověď na ověření (jen BLE)
//   CSM CFG blind_ms=<ms>              potvrzení změny slepé doby
//   CSM ERR <text>                     chyba příkazu
//   CSM PONG                           odpověď na PING (sledování živosti UART)
//   CSM WIFI connecting|fail|off       stav Wi-Fi (OTA aktualizace)
//   CSM WIFI ok ip=<ip> token=<t>      připojeno; token opravňuje k POST /ota
//   CSM OTA start size=<B>|done|fail   průběh OTA zápisu
// Příkazy web -> deska (řádky ukončené '\n'):
//   r / R                              nový start (re-arm)
//   AUTH <pin>                         ověření (BLE; USB je důvěryhodné fyzicky)
//   SET blind_ms <ms>                  změna slepé doby (uloží se do NVS)
//   PING                               keep-alive (web posílá ~1x/s)
//   WIFI <ssid>\t<heslo>               připojit Wi-Fi pro OTA (creds jen v RAM)
//   WIFI OFF                           vypnout Wi-Fi
//
// Ověření: USB UART je důvěryhodné (fyzický přístup ke kabelu). BLE klient musí
// nejdřív poslat "AUTH <pin>"; do té doby se jeho příkazy ignorují. Jde o prostou
// aplikační bariéru - BLE provoz není šifrovaný, takže to není ochrana proti
// odposlechu, ale proti nechtěnému ovládání cizí osobou.
// Výchozí hodnoty (PIN, název BLE, slepá doba) se nastavují v `idf.py menuconfig`
// -> Casomira. Slepá doba jde měnit i za běhu (SET) a přežije restart (NVS).
#define PROTO_VERSION 1

// ================== Stav časomíry ==================
// IDLE = zamčeno: po zapnutí napájení a po každé změně režimu připojení.
// Měření se musí nejdřív povolit (tlačítko v ručním režimu / 'r' z webu),
// jinak by časomíru odpálilo jakékoli přerušení paprsku hned po zapojení.
typedef enum { IDLE, READY, RUNNING, DONE } state_t;

static volatile state_t  s_state     = IDLE;
static volatile int64_t  s_start_us  = 0;   // čas startu  [µs] z esp_timer
static volatile int64_t  s_stop_us   = 0;   // čas cíle    [µs] z esp_timer
static volatile bool     s_rearm_req = false; // žádost o nový start (z UART/BLE)
static volatile bool     s_ble_authed = false; // BLE klient ověřen PINem?

// Slepá doba po startu: default z Kconfigu, přepisovatelná za běhu (SET) a
// persistentní v NVS. ISR čte s_blind_us; s_blind_ms je stejná hodnota v ms.
static uint32_t          s_blind_ms   = CONFIG_CASOMIRA_BLIND_MS;
static volatile int64_t  s_blind_us   = (int64_t)CONFIG_CASOMIRA_BLIND_MS * 1000;

// Sledování spojení pro OLED / ruční režim.
typedef enum { CONN_NONE, CONN_UART, CONN_BLE } conn_t;
static volatile bool     s_ble_connected = false; // navázán GATT spoj (BLE)
static volatile uint32_t s_uart_last_ms  = 0;     // čas poslední zprávy z UART [ms]
static int64_t           s_last_result_us = -1;   // poslední změřený čas (nebo -1)
static bool              s_oled_ok = false;

// Kritická sekce pro atomické přezbrojení časomíry vůči ISR.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

// Aktuální stav připojení (BLE má přednost, pak živý UART, jinak ruční režim).
static conn_t current_conn(void)
{
    if (s_ble_connected) return CONN_BLE;
    if (s_uart_last_ms != 0 && (now_ms() - s_uart_last_ms) < UART_ALIVE_MS) return CONN_UART;
    return CONN_NONE;
}

// --- ISR: přesný čas hrany sejmeme přímo v přerušení (rozlišení 1 µs) ---
static void IRAM_ATTR beam_isr(void *arg)
{
    int64_t now = esp_timer_get_time();

    switch (s_state) {
        case IDLE:                                  // zamčeno - pulzy ignorujeme
            break;

        case READY:                                 // 1. přerušení = START
            s_start_us = now;
            s_state = RUNNING;
            break;

        case RUNNING:                               // běžíme
            if (now - s_start_us >= s_blind_us) {
                s_stop_us = now;                    // platné cílové přerušení = STOP
                s_state = DONE;
            }
            // jinak jsme ve slepé době -> pulz ignorujeme
            break;

        case DONE:
            break;                                  // hotovo, čekáme na reset
    }
}

// ================== Pomocné funkce ==================

// Odešle jeden řádek protokolu po obou kanálech: UART (stdout) i BLE.
// Deklarace v proto.h - volá se i z ota_wifi.c (eventy Wi-Fi/OTA).
void csm_emit(const char *fmt, ...)
{
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(buf)) {
        n = sizeof(buf) - 1;   // ořízlé (nemělo by nastat, řádky jsou krátké)
    }
    fwrite(buf, 1, n, stdout);
    fflush(stdout);
    ble_uart_send(buf, n);
}

// Nastaví slepou dobu [ms] do RAM i NVS (persistentně přes restart).
static void set_blind_ms(uint32_t ms)
{
    s_blind_ms = ms;
    s_blind_us = (int64_t)ms * 1000;

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, NVS_KEY_BLIND, ms);
        nvs_commit(h);
        nvs_close(h);
    }
}

// Načte slepou dobu z NVS (pokud tam je), jinak nechá default z Kconfigu.
static void load_blind_ms(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        uint32_t v;
        if (nvs_get_u32(h, NVS_KEY_BLIND, &v) == ESP_OK) {
            s_blind_ms = v;
            s_blind_us = (int64_t)v * 1000;
        }
        nvs_close(h);
    }
}

// Vykoná příkaz od oprávněného klienta (r, SET, PING). Volá se z UART i BLE.
static void exec_command(const char *line)
{
    if (strcmp(line, "PING") == 0) {
        csm_emit("CSM PONG\n");
        return;
    }
    if (strcmp(line, "r") == 0 || strcmp(line, "R") == 0) {
        s_rearm_req = true;
        return;
    }
    if (strncmp(line, "SET blind_ms ", 13) == 0) {
        int v = atoi(line + 13);
        if (v >= 200 && v <= 60000) {           // rozumné meze [ms]
            set_blind_ms((uint32_t)v);
            csm_emit("CSM CFG blind_ms=%u\n", (unsigned)s_blind_ms);
        } else {
            csm_emit("CSM ERR blind_ms mimo rozsah 200-60000\n");
        }
        return;
    }
    if (strncmp(line, "WIFI ", 5) == 0) {
        const char *arg = line + 5;
        if (strcmp(arg, "OFF") == 0) {
            ota_wifi_off();
            return;
        }
        // Formát: WIFI <ssid>\t<heslo> (tab - SSID může obsahovat mezery)
        const char *tab = strchr(arg, '\t');
        if (tab == NULL || tab == arg) {
            csm_emit("CSM ERR wifi format (SSID<TAB>heslo)\n");
            return;
        }
        char ssid[33] = {0};
        char pass[65] = {0};
        size_t sl = (size_t)(tab - arg);
        if (sl > sizeof(ssid) - 1) sl = sizeof(ssid) - 1;
        memcpy(ssid, arg, sl);
        strlcpy(pass, tab + 1, sizeof(pass));
        ota_wifi_connect(ssid, pass);
        return;
    }
    csm_emit("CSM ERR neznamy prikaz\n");
}

// Zpracuje jeden celý příkazový řádek z BLE (bez '\n'). Řeší i ověření.
static void handle_ble_command(const char *line)
{
    // AUTH <pin> - ověření klienta
    if (strncmp(line, "AUTH ", 5) == 0) {
        if (strcmp(line + 5, CONFIG_CASOMIRA_PIN) == 0) {
            s_ble_authed = true;
            csm_emit("CSM AUTH ok\n");
        } else {
            s_ble_authed = false;
            csm_emit("CSM AUTH fail\n");
        }
        return;
    }
    // PING projde i bez ověření (keep-alive).
    if (strcmp(line, "PING") == 0) {
        exec_command(line);
        return;
    }
    // Ostatní příkazy vyžadují ověření.
    if (!s_ble_authed) {
        csm_emit("CSM AUTH required\n");
        return;
    }
    exec_command(line);
}

// Příchozí bajty z BLE skládáme do řádků (ukončených '\n') a předáváme dál.
// 160 B: musí pojmout i "WIFI <ssid 32>\t<heslo 63>".
static void on_ble_rx(const uint8_t *data, size_t len)
{
    static char   buf[160];
    static size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if (c == '\n' || c == '\r') {
            if (n > 0) {
                buf[n] = '\0';
                handle_ble_command(buf);
                n = 0;
            }
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = c;
        } else {
            n = 0;   // přetečení řádku -> zahodíme
        }
    }
}

// (Od)pojení BLE klienta: sledujeme spojení, při odpojení zrušíme ověření.
static void on_ble_conn(bool connected)
{
    s_ble_connected = connected;
    if (!connected) {
        s_ble_authed = false;
    }
}

// HW pin glitch filtr: zahodí elektrické zákmity kratší než ~2 cykly
// hodin IO-MUX (jednotky až desítky ns) přímo v křemíku GPIO.
static void setup_glitch_filter(void)
{
    gpio_glitch_filter_handle_t filter = NULL;
    gpio_pin_glitch_filter_config_t cfg = {
        .clk_src  = GLITCH_FILTER_CLK_SRC_DEFAULT,
        .gpio_num = SENSOR_PIN,
    };
    esp_err_t err = gpio_new_pin_glitch_filter(&cfg, &filter);
    if (err == ESP_OK) {
        gpio_glitch_filter_enable(filter);
        ESP_LOGI(TAG, "HW glitch filtr: aktivni.");
    } else {
        ESP_LOGW(TAG, "HW glitch filtr: chyba %s", esp_err_to_name(err));
    }
}

static void arm_timer(void)
{
    portENTER_CRITICAL(&s_mux);
    s_state = READY;
    portEXIT_CRITICAL(&s_mux);
    csm_emit("CSM READY\n");
}

// Konzolová UART pro čtení příkazu 'r' (nový start).
// UART0 už je nakonfigurovaná zaváděcím kódem (115200 Bd), takže NEvoláme
// uart_param_config - reset jádra UART uprostřed dobíhajícího výpisu logu by
// rozházel právě odesílané znaky. Stačí počkat na dovysílání a doinstalovat
// ovladač, aby šlo číst příchozí bajty.
static void console_init(void)
{
    fflush(stdout);
    esp_rom_output_tx_wait_idle(CONSOLE_UART);   // necháme dojet TX FIFO
    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0));
}

// Skládá příchozí bajty z konzole (UART0) do řádků a vykonává příkazy.
// Konzole je důvěryhodná (fyzický přístup), ověření se neřeší. Jakýkoli
// příjem zároveň dokazuje živé spojení (pro ruční režim / OLED).
static void console_feed(const uint8_t *data, size_t len)
{
    static char   buf[160];   // musí pojmout i "WIFI <ssid>\t<heslo>"
    static size_t n = 0;
    if (len > 0) s_uart_last_ms = now_ms();
    for (size_t i = 0; i < len; i++) {
        char ch = (char)data[i];
        if (ch == '\n' || ch == '\r') {
            if (n > 0) {
                buf[n] = '\0';
                exec_command(buf);
                n = 0;
            }
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = ch;
        } else {
            n = 0;   // přetečení řádku -> zahodíme
        }
    }
}

// Naformátuje čas [µs] do "MM:SS.mmm".
static void fmt_time(int64_t us, char *buf, size_t n)
{
    int64_t ms = (us + 500) / 1000;
    unsigned mm = (unsigned)(ms / 60000);
    unsigned ss = (unsigned)((ms / 1000) % 60);
    unsigned mmm = (unsigned)(ms % 1000);
    snprintf(buf, n, "%02u:%02u.%03u", mm, ss, mmm);
}

// Překreslí OLED, ale jen když se obsah změnil (šetří I2C provoz).
// Layout (bez zbytečných popisků, 128x64):
//   řádek 0  (8 px)  : režim spojení (UART / BLUETOOTH / RUCNI)
//   střed    (24 px) : ČAS velkým fontem přes celou šířku
//                      - READY:   00:00.0
//                      - RUNNING: živý čas s rozlišením 0,1 s
//                      - DONE:    přesný výsledek MM:SS.mmm
//   řádek 56 (8 px)  : nápověda (např. "TLACITKO = RESET" v ručním režimu)
static void display_update(void)
{
    if (!s_oled_ok) return;

    conn_t c = current_conn();
    const char *mode = (c == CONN_BLE) ? "BLUETOOTH"
                     : (c == CONN_UART) ? "UART" : "RUCNI";

    char big[16];
    const char *hint = "";
    if (s_state == RUNNING) {
        int64_t ds = (esp_timer_get_time() - s_start_us) / 100000;  // desetiny sekundy
        snprintf(big, sizeof(big), "%02u:%02u.%u",
                 (unsigned)(ds / 600), (unsigned)((ds / 10) % 60), (unsigned)(ds % 10));
    } else if (s_state == DONE && s_last_result_us >= 0) {
        fmt_time(s_last_result_us, big, sizeof(big));               // MM:SS.mmm
        hint = (c == CONN_NONE) ? "TLACITKO = RESET" : "RESET Z WEBU";
    } else if (s_state == IDLE) {  // zamčeno - čeká na povolení
        snprintf(big, sizeof(big), "00:00.0");
        hint = (c == CONN_NONE) ? "STISKNI TLACITKO" : "POVOL Z WEBU";
    } else {  // READY
        snprintf(big, sizeof(big), "00:00.0");
        hint = "CEKAM NA START";
    }

    // Překresli jen při změně obsahu (živý čas se mění 10x/s sám od sebe).
    static char last[48] = "";
    char cur[48];
    snprintf(cur, sizeof(cur), "%s|%s|%s", mode, big, hint);
    if (strcmp(cur, last) == 0) return;
    strcpy(last, cur);

    ssd1306_clear();
    ssd1306_text_centered(0, mode, 1);
    ssd1306_bigtext_centered(18, big);
    ssd1306_text_centered(56, hint, 1);
    ssd1306_flush();
}

// ================== app_main ==================
void app_main(void)
{
    // Pin jako vstup s pull-up (optozávora spíná do L, klidová úroveň H).
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << SENSOR_PIN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,          // náběžná hrana = přerušení paprsku
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    // Pořadí: nejdřív pin jako vstup, pak filtr, pak přerušení.
    setup_glitch_filter();

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(SENSOR_PIN, beam_isr, NULL));

    // Tlačítko pro ruční reset (spíná do GND, klidová úroveň H).
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BUTTON_PIN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn));

    // OLED displej (nepovinný - když na I2C neodpoví, běží se bez něj).
    // Obsah kreslí display_update() ve smyčce, žádný uvítací text.
    s_oled_ok = ssd1306_init(I2C_SDA_PIN, I2C_SCL_PIN, OLED_ADDR);
    if (!s_oled_ok) {
        ESP_LOGW(TAG, "OLED nenalezen - pokracuji bez displeje");
    }

    console_init();

    // NVS je potřeba pro BLE (ukládá kalibrace/klíče) i pro naše nastavení.
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    load_blind_ms();   // přepíše default z Kconfigu, pokud je v NVS uložená změna

    ble_uart_set_rx_cb(on_ble_rx);
    ble_uart_set_conn_cb(on_ble_conn);
    ble_uart_init(CONFIG_CASOMIRA_BLE_NAME);

    // Uvítací řádek protokolu - potvrzení linky pro webové rozhraní.
    csm_emit("CSM HELLO proto=%d blind_ms=%u\n", PROTO_VERSION, (unsigned)s_blind_ms);

    // Záměrně NEvyzbrojujeme: po zapnutí je časomíra zamčená (IDLE) a čeká
    // na povolení tlačítkem (ruční režim) nebo příkazem 'r' z webu.

    bool started  = false;
    bool reported = false;
    conn_t prev_conn = CONN_NONE;
    while (1) {
        // Změna režimu připojení (RUCNI <-> UART <-> BLUETOOTH) anuluje měření:
        // časomíra se zamkne a čeká na nové povolení. Jeden typ připojení tak
        // nikdy nezdědí rozběhnuté měření po jiném.
        conn_t conn = current_conn();
        if (conn != prev_conn) {
            prev_conn = conn;
            portENTER_CRITICAL(&s_mux);
            s_state = IDLE;
            portEXIT_CRITICAL(&s_mux);
            s_last_result_us = -1;
            started  = false;
            reported = false;
            csm_emit("CSM IDLE\n");
        }

        // Hned po startu (1. přerušení paprsku) oznámíme začátek kola -
        // i během slepé doby, kdy se další pulzy ignorují.
        if (s_state != READY && !started) {
            csm_emit("CSM START us=%lld\n", (long long)s_start_us);
            started = true;
        }

        // Výsledek (přesné trvání kola v µs) jednou po dokončení běhu.
        if (s_state == DONE && !reported) {
            s_last_result_us = s_stop_us - s_start_us;
            csm_emit("CSM RESULT us=%lld\n", (long long)s_last_result_us);
            reported = true;
        }

        // Příkazy z konzole (UART0).
        uint8_t rx[16];
        int n = uart_read_bytes(CONSOLE_UART, rx, sizeof(rx), pdMS_TO_TICKS(50));
        if (n > 0) console_feed(rx, n);

        // Tlačítko: v ručním režimu (bez připojení) odemkne a resetuje časomíru.
        int lvl = gpio_get_level(BUTTON_PIN);
        static bool btn_up = true;
        if (lvl == 0 && btn_up) {              // sestupná hrana = stisk
            btn_up = false;
            if (current_conn() == CONN_NONE) s_rearm_req = true;
        } else if (lvl == 1) {
            btn_up = true;
        }

        // Reset / re-arm pro dalšího závodníka (z UART, BLE i tlačítka).
        if (s_rearm_req) {
            s_rearm_req = false;
            started  = false;
            reported = false;
            arm_timer();
        }

        display_update();   // překreslí OLED jen při změně obsahu
    }
}
