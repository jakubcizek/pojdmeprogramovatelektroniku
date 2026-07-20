#include <string.h>

#include "sdkconfig.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"

#include "ssd1306.h"
#include "ssd1306_font.h"
#include "ssd1306_bigfont.h"

#define OLED_W 128
#define OLED_H 64
#define OLED_PAGES (OLED_H / 8)

// SH1106 má RAM 132 sloupcu, panel 128 px je uprostred -> zapis s offsetem 2.
#ifdef CONFIG_CASOMIRA_OLED_SH1106
#define COL_OFFSET 2
static const char *TAG = "oled(sh1106)";
#else
#define COL_OFFSET 0
static const char *TAG = "oled(ssd1306)";
#endif

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static uint8_t s_fb[OLED_W * OLED_PAGES];   // framebuffer (1 bit/pixel, sloupcově po stránkách)

// Inicializační sekvence (Co=0 -> po řídicím bajtu 0x00 jdou samé příkazy).
// Společná pro SSD1306 i SH1106; liší se jen nábojovou pumpou. Adresování
// se nenastavuje - flush používá stránkový zápis, který umí oba řadiče.
static const uint8_t INIT_CMDS[] = {
    0x00,
    0xAE,             // display off
    0xD5, 0x80,       // clock div
    0xA8, 0x3F,       // multiplex 1/64
    0xD3, 0x00,       // display offset 0
    0x40,             // start line 0
#ifdef CONFIG_CASOMIRA_OLED_SH1106
    0xAD, 0x8B,       // SH1106: DC-DC (nábojová pumpa) zapnout
#else
    0x8D, 0x14,       // SSD1306: charge pump on
#endif
    0xA1,             // segment remap
    0xC8,             // COM scan direction
    0xDA, 0x12,       // COM pins
    0x81, 0xFF,       // contrast: maximum (jas OLED)
    0xD9, 0xF1,       // pre-charge
    0xDB, 0x40,       // VCOM detect
    0xA4,             // resume from RAM
    0xA6,             // normal (non-inverted)
    0xAF,             // display on
};

// ---- kreslení do framebufferu ----
static inline void set_px(int x, int y)
{
    if ((unsigned)x < OLED_W && (unsigned)y < OLED_H) {
        s_fb[(y >> 3) * OLED_W + x] |= (uint8_t)(1u << (y & 7));
    }
}

static void draw_char(int x, int y, char ch, int scale)
{
    if (ch < 0x20 || ch > 0x7E) ch = '?';
    const uint8_t *g = SSD1306_FONT8[ch - 0x20];
    for (int c = 0; c < 8; c++) {
        uint8_t col = g[c];
        for (int r = 0; r < 8; r++) {
            if (col & (1u << r)) {
                for (int dx = 0; dx < scale; dx++)
                    for (int dy = 0; dy < scale; dy++)
                        set_px(x + c * scale + dx, y + r * scale + dy);
            }
        }
    }
}

int ssd1306_text_width(const char *s, int scale)
{
    return (int)strlen(s) * 8 * scale;
}

void ssd1306_text(int x, int y, const char *s, int scale)
{
    for (; *s; s++) {
        draw_char(x, y, *s, scale);
        x += 8 * scale;
    }
}

void ssd1306_text_centered(int y, const char *s, int scale)
{
    int x = (OLED_W - ssd1306_text_width(s, scale)) / 2;
    if (x < 0) x = 0;
    ssd1306_text(x, y, s, scale);
}

// ---- velký font (jen "0123456789:.") ----
static int big_index(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch == ':') return 10;
    if (ch == '.') return 11;
    return -1;
}

int ssd1306_bigtext_width(const char *s)
{
    int w = 0;
    for (; *s; s++) {
        int i = big_index(*s);
        if (i >= 0) w += BIGFONT_W[i] + 1;   // 1 px mezera
    }
    return (w > 0) ? w - 1 : 0;
}

void ssd1306_bigtext(int x, int y, const char *s)
{
    for (; *s; s++) {
        int i = big_index(*s);
        if (i < 0) continue;
        int w = BIGFONT_W[i];
        for (int c = 0; c < w; c++) {
            uint32_t col = (uint32_t)BIGFONT[i][c * 3]
                         | ((uint32_t)BIGFONT[i][c * 3 + 1] << 8)
                         | ((uint32_t)BIGFONT[i][c * 3 + 2] << 16);
            for (int r = 0; r < BIGFONT_H; r++) {
                if (col & (1u << r)) set_px(x + c, y + r);
            }
        }
        x += w + 1;
    }
}

void ssd1306_bigtext_centered(int y, const char *s)
{
    int x = (OLED_W - ssd1306_bigtext_width(s)) / 2;
    if (x < 0) x = 0;
    ssd1306_bigtext(x, y, s);
}

void ssd1306_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
}

void ssd1306_flush(void)
{
    if (s_dev == NULL) return;

    // Stránkový zápis (umí SSD1306 i SH1106; SH1106 horizontal mode nemá):
    // pro každou stránku nastav adresu a pošli 128 bajtů dat.
    uint8_t chunk[1 + OLED_W];
    for (int p = 0; p < OLED_PAGES; p++) {
        const uint8_t cmds[] = {
            0x00,
            (uint8_t)(0xB0 | p),                       // stránka
            (uint8_t)(0x00 | (COL_OFFSET & 0x0F)),     // sloupec - dolní nibble
            (uint8_t)(0x10 | (COL_OFFSET >> 4)),       // sloupec - horní nibble
        };
        if (i2c_master_transmit(s_dev, cmds, sizeof(cmds), 1000) != ESP_OK) return;

        chunk[0] = 0x40;                               // řídicí bajt: data
        memcpy(&chunk[1], &s_fb[p * OLED_W], OLED_W);
        if (i2c_master_transmit(s_dev, chunk, sizeof(chunk), 1000) != ESP_OK) return;
    }
}

// Projede celý rozsah I2C adres a vypíše do logu, co odpovídá. Pomůcka pro
// ladění zapojení: hned je vidět, jestli je displej na jiné adrese nebo jestli
// se sběrnice vůbec nechytá (špatné piny / chybějící pull-upy).
void ssd1306_i2c_scan(void)
{
    if (s_bus == NULL) return;
    ESP_LOGI(TAG, "I2C sken (adresy 0x08-0x77):");
    int found = 0;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(s_bus, a, 30) == ESP_OK) {
            ESP_LOGI(TAG, "  -> zarizeni na 0x%02X", a);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "  zadne zarizeni. Zkontroluj SDA/SCL piny, spolecnou zem a pull-up rezistory (4.7k na 3V3).");
    } else {
        ESP_LOGI(TAG, "  celkem nalezeno: %d", found);
    }
}

bool ssd1306_init(int sda_gpio, int scl_gpio, uint8_t i2c_addr)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,                      // -1 = vyber volný port
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) {
        ESP_LOGW(TAG, "i2c_new_master_bus selhalo");
        return false;
    }
    ESP_LOGI(TAG, "I2C sbernice: SDA=%d, SCL=%d", sda_gpio, scl_gpio);

    ssd1306_i2c_scan();   // při každém bootu vypíšeme, co je na sběrnici (ladění)

    esp_err_t probe = i2c_master_probe(s_bus, i2c_addr, 100);
    if (probe != ESP_OK) {
        ESP_LOGW(TAG, "OLED na adrese 0x%02X neodpovida (%s)", i2c_addr, esp_err_to_name(probe));
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 400000,   // 400 kHz: svižný refresh (živý čas na displeji)
    };
    if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev) != ESP_OK) {
        ESP_LOGW(TAG, "add_device selhalo");
        return false;
    }

    if (i2c_master_transmit(s_dev, INIT_CMDS, sizeof(INIT_CMDS), 1000) != ESP_OK) {
        ESP_LOGW(TAG, "init sekvence selhala");
        return false;
    }

    ssd1306_clear();
    ssd1306_flush();
    return true;
}
