#include "esp_timer.h"          // Měření času jednotlivých úloh
#include "esp_log.h"            // Logování skrze USB UART
#include "esp_http_server.h"    // HTTP server
#include "freertos/semphr.h"    // Semafor, který řídí přístup ke kameře

// KOMPONENTY PROJEKTU
// Aby byl kód lépe čitelný, vytvořili jsme si několik komponent/knihoven,
// které se starají o specifickou práci a jsou přenositelné i do dalších projektů
// Komponenty najdete v podadresáři /components
#include "ethernet.h"        // Ethernet se stará o připojení k síti. ESP32-P4 podporuje MAC, PHY dodá čip IP101 na desce
#include "gfx.h"             // GFX se stará o kreslení primitivních tvarů a písma. Můžete rozšířit o další tvary
#include "camera.h"          // Camera se stará o komunikaci s ovladačem kamery a na povel získá snímek v RGB565
#include "jpeg.h"            // JPEG se stará o kódování RGB565 do JPEG

#include "fonts/roboto60.h"  // V komponentě GFX najdete podadresář fonts s rastrovými písmy, které můžete vytvářet ve skriptu fontgenerator.py
#include "fonts/roboto30.h"
#include "html.h"            // HTML kód úvodní stránky

static const char *TAG = "Main";

// Pomocný semafor, který řídí přístup ke kameře, aby nedošlo ke kolizi
static SemaphoreHandle_t camera_lock;  

// Deklarace funkcí v tomto souboru
static esp_err_t start_webserver_control(void);                      // Nastartuj primární kontrolní/API server (TCP port 80)
static esp_err_t start_webserver_mjpeg(void);                        // Nastartuj sekundární streamovací server (TCP port 81)

// Webhandlery, tedy funkce, které se volají při nejrůznějších HTTP GET dotazech/cestách
static esp_err_t webhandler_index(httpd_req_t *req);                 // Kořen /
static esp_err_t webhandler_capture_jpeg(httpd_req_t *http_req);     // /capture-jpeg
static esp_err_t webhandler_mjpeg_loop(httpd_req_t *http_req);       // Kořen / na sekundárním streamovacím serveru se smyčkou

//-----------------------------------------------------------------------------
// Hlavní funkce a začátek programu. Analogie setup() ze světa Arduino
//-----------------------------------------------------------------------------
void app_main(void){

    // Připojíme se k síti skrze ethernet
    // Toto se děje asynchronně, neblokujeme tok programu!
    ethernet_connect();

    // Semafor pro kameru. Určuje, jestli z ní můžeme právě číst, nebo ji blokuje jiný proces
    camera_lock = xSemaphoreCreateBinary();
    xSemaphoreGive(camera_lock);

    // Nastartujeme primární/kontrolní a sekundární/streamovací server
    start_webserver_control();
    start_webserver_mjpeg();

    // Vše je připraveno, můžete otevřít v prohlížeči:
    // http://<ip adresa> pro zobrazení ovládací stránky
    // http://<ip adresa>/capture-jpeg pro okamžité vytvoření JPEG obrázku
    // http://<ip adresa>:81 pro zobrazení MJPEG streamu
}

//-----------------------------------------------------------------------------
// Webhandler pro kořen / na primárním serveru
// Vrátíme stringovou konstantu INDEX_HTML ze souboru html.h
//-----------------------------------------------------------------------------
static esp_err_t webhandler_index(httpd_req_t *http_req){
    ESP_LOGI(TAG, "HTTP GET: /");
    httpd_resp_set_type(http_req, "text/html; charset=utf-8");
    // Žádné kešování na straně ptohlížeče!
    httpd_resp_set_hdr(http_req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(http_req, "Pragma", "no-cache");
    httpd_resp_set_hdr(http_req, "Expires", "0");
    httpd_resp_send(http_req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

//-----------------------------------------------------------------------------
// Webhandler pro /capture-jpeg na primárním serveru
//-----------------------------------------------------------------------------
static esp_err_t webhandler_capture_jpeg(httpd_req_t *http_req){
    ESP_LOGI(TAG, "HTTP GET: /capture-jpeg");

    // Výchozí zahřívací čas po spuštění kamery a JPEG kvalita 
    int seconds = 1, quality = 80;

    // Budeme měřit čas jednotlivých fází v mikrosekundách
    int64_t duration_capture = 0;
    int64_t duration_gfx = 0;
    int64_t duration_jpeg = 0;
    int64_t duration_transport = 0;
    int64_t duration_warming = 0;

    // Zjistíme, jestli URL obsahuje argumenty ?quality=XXX&seconds=XXX
    int qlen = httpd_req_get_url_query_len(http_req);
    if (qlen > 0) {
        char qbuf[128];
        if (qlen < (int)sizeof(qbuf) && httpd_req_get_url_query_str(http_req, qbuf, sizeof(qbuf)) == ESP_OK) {
            char val[16];
            if (httpd_query_key_value(qbuf, "quality", val, sizeof(val)) == ESP_OK) {
                int v = atoi(val);
                if (v >= 1 && v <= 100) quality = v;
            }
            if (httpd_query_key_value(qbuf, "seconds", val, sizeof(val)) == ESP_OK) {
                int v = atoi(val);
                if (v >= 0 && v <= 10) seconds = v;
            }
        }
    }

    ESP_LOGI(TAG, "HTTP URL argument quality: %d", quality);
    ESP_LOGI(TAG, "HTTP URL argument seconds: %d", seconds);

    // Pomocí semaforu zjistíme, jestli se právě pracuje s kamerou
    // Pokud ano, vrátíme chybový HTTP kód, anebo si ji blokujeme pro sebe
    if (xSemaphoreTake(camera_lock, 0) != pdTRUE) {
        httpd_resp_set_status(http_req, "503 Service Unavailable");
        httpd_resp_set_type(http_req, "text/plain");
        httpd_resp_sendstr(http_req, "Kamera je zaneprázdněna\n");
        return ESP_OK;
    }

    // Nastartujeme kameru a pro provozní data (framebuffer atp.)
    // použijeme strukturu CameraContext ze shared_types.h
    // Pokud se to nepodaří, vrátíme chybový HTTP kód a uvolníme semafor 
    CameraContext cc;
    if (camera_ctx_begin(&cc) != ESP_OK) {
        xSemaphoreGive(camera_lock);
        httpd_resp_set_status(http_req, "500 Internal Server Error");
        httpd_resp_set_type(http_req, "text/plain");
        httpd_resp_sendstr(http_req, "Inicializace kamery selhala\n");
        return ESP_OK;
    }

    // Nastartujeme JPEG kodér
    // Pokud se to nepodaří, zastavíme kameru, uvolníme semafor
    // a vrátíme chybový HTTP kód
    JpegCtx jc;
    if (jpeg_ctx_begin(&jc, cc.width, cc.height, quality) != ESP_OK) {
        camera_ctx_end(&cc);
        xSemaphoreGive(camera_lock);
        httpd_resp_set_status(http_req, "500 Internal Server Error");
        httpd_resp_set_type(http_req, "text/plain");
        httpd_resp_sendstr(http_req, "Inicializace JPEG kodéru selhala\n");
        return ESP_OK;
    }

    // Zahřívací smyčka podle počtu stanovených sekund
    // Předpokládáme, že po startu kamery může chvíli trvat, než se obraz stabilizuje
    // Volitelně tedy prvních N sekund čteme a zahazujeme snímky
    // Je třeba ještě prostudovat, jak se to chová v praxi. Inspiroval jsem se na Raspberry Pi
    int32_t t = esp_timer_get_time();
    if(seconds > 0){
        while (esp_timer_get_time() - t < (int64_t)seconds * 1000000LL) {
            if (camera_ctx_get_frame(&cc) == ESP_OK) {
                camera_ctx_requeue(&cc);
            } else {
                break;
            }
            vTaskDelay(1);
        }
    }
    duration_warming = esp_timer_get_time() - t;
    ESP_LOGI(TAG, "Zahřívání kamery trvalo %" PRId64 " us", duration_warming);


    // Přečteme snímek z kamery ve formátu RGB565
    // Ve skutečnosti nečteme snímek přímo z kamery, ale z dalšího násobného cyklického bufferu
    // Ten driver přepisuje neustále dokola a my si vždy vezmeme ten hotový = právě proto jich musí být více!
    // V opačném případě by se mohlo stát, že budeme číst snímek, který se už částečně přepisuje novými daty
    // Pokud se čtení snímku nepodaří, zastavíme kameru, uvolníme semafor a vrátíme chybový HTTP kód
    t = esp_timer_get_time();
    if (camera_ctx_get_frame(&cc) != ESP_OK) {
        camera_ctx_end(&cc);
        xSemaphoreGive(camera_lock);
        httpd_resp_set_status(http_req, "500 Internal Server Error");
        httpd_resp_sendstr(http_req, "Nemohu získat snímek z kamery\n");
        return ESP_OK;
    }
    duration_capture = esp_timer_get_time() - t;
    ESP_LOGI(TAG, "Získání RGB565 snímku trvalo %" PRId64 " us", duration_capture);

    // V TÉTO FÁZI UŽ MÁME V PSRAM BEZPEČNĚ FRAMEBUFFER SNÍMKU VE FORMÁTU RGB565
    // RGB565 volím proto, že nad ním mohu provádět čitelné úpravy pixelů
    // RGB565 je intuitivnější než YUV apod. a používá se i v jiných knihovnách pro GFX,
    // které bych mohl napojit do projektu 

    // Pomocí naší GFX komponenty nakreslím do framebufferu trojici rámečků s okrajem 5px
    // Rozměry framebufferu jsou ve struktuře CameraContext (.H pro výšku a .W pro šířku)
    // GFX operace budou trvat nižší jednotky milisekund
    t = esp_timer_get_time();
    gfx_draw_rect(&cc, 10,10, cc.width-20, cc.height-20, 255,255,255, 5);
    gfx_draw_rect(&cc, 15,15, cc.width-30, cc.height-30, 0, 0, 0, 5);
    gfx_draw_rect(&cc, 20,20, cc.width-40, cc.height-40, 255, 255, 255, 5);

    // Dále pomocí GFX knihovny vypíšu do záhlaví hlášku
    // Aby byla vycentrovaná na střed, nejprve spočítám šířku textu s vybraným rastrovým fontem
    int tw = gfx_get_text_width(&Roboto60, "ESP32-P4 foťáček");
    int x = (int)cc.width/2 - tw/2;
    // První text bude černý a mírně posunutý, druhý bílý
    // Dosáhnu tak efektu primitivního stínu
    gfx_print(&cc, x+5, 55, &Roboto60, "ESP32-P4 foťáček", 0,0,0);
    gfx_print(&cc, x, 50, &Roboto60, "ESP32-P4 foťáček", 255,255,255);
    duration_gfx = esp_timer_get_time() - t;
    ESP_LOGI(TAG, "GFX kreslení trvalo %" PRId64 " us", duration_gfx);

    // Práce s framebufferem je hotová, a tak ho pošleme do JPEG kodéru
    // Výstupem je už bajtové pole, zatímco RGB565 framebuffer používá uint16_t pole (jeden záznam = 1 pixel) 
    uint8_t *jpg; size_t jpg_len;
    t = esp_timer_get_time();
    esp_err_t enc = jpeg_ctx_encode(&jc, cc.frame_buffer_rgb565, &jpg, &jpg_len);
    duration_jpeg = esp_timer_get_time() - t;
    ESP_LOGI(TAG, "Kódování JPEG trvalo %" PRId64 " us", duration_jpeg);

    // Ještě ale mohlo dojít k chybě při JPEG kódování, a tak provedeme poslední kontrolu,
    // že je vše v pořádku. V opačném případě bychom se mohli pokusit o další kódování
    if (enc != ESP_OK || !jpg || !jpg_len) {
        camera_ctx_requeue(&cc);
        camera_ctx_end(&cc);
        jpeg_ctx_end(&jc);
        httpd_resp_set_status(http_req, "500 Internal Server Error");
        httpd_resp_sendstr(http_req, "Kódování JPEG selhalo\n");
        return ESP_OK;
    }

    // Bajty obrázku jsou hotové, a tak sestavíme HTTP odpověď
    // Do uživatelských "X-" hlavíček uložíme statistiku o době trvání dílčích úloh
    // Může se to hodit pro testování
    char str_duration_capture[20];
    char str_duration_gfx[20];
    char str_duration_jpeg[20];
    char str_duration_warming[20];
    snprintf(str_duration_capture, sizeof(str_duration_capture)-1, "%" PRId64, duration_capture);
    snprintf(str_duration_gfx, sizeof(str_duration_gfx)-1, "%" PRId64, duration_gfx);
    snprintf(str_duration_jpeg, sizeof(str_duration_jpeg)-1, "%" PRId64, duration_jpeg);
    snprintf(str_duration_warming, sizeof(str_duration_warming)-1, "%" PRId64, duration_warming);

    httpd_resp_set_type(http_req, "image/jpeg");
    httpd_resp_set_hdr(http_req, "X-Duration-Warming", str_duration_warming);
    httpd_resp_set_hdr(http_req, "X-Duration-Capture", str_duration_capture);
    httpd_resp_set_hdr(http_req, "X-Duration-Gfx", str_duration_gfx);
    httpd_resp_set_hdr(http_req, "X-Duration-Jpeg", str_duration_jpeg);

    // Žádné kešování na straně ptohlížeče!
    httpd_resp_set_hdr(http_req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(http_req, "Pragma", "no-cache");
    httpd_resp_set_hdr(http_req, "Expires", "0");

    // Odešleme data klientovi
    t = esp_timer_get_time();
    if(httpd_resp_send_chunk(http_req, (const char*)jpg, jpg_len) != ESP_OK){
        ESP_LOGE(TAG, "Nepodařilo se odeslat JPEG data HTTP klientovi");
    }
    duration_transport = esp_timer_get_time() - t;
    ESP_LOGI(TAG, "Odesílání dat trvalo %" PRId64 " us", duration_transport);

    // Na závěr ukončíme práci s kamerou, která smaže i strukturu CameraContext s framebuffery
    // Stejně tak uvolníme prostředky JPEG enkodéru a vrátíme semafor
    jpeg_ctx_end(&jc);
    camera_ctx_requeue(&cc);
    camera_ctx_end(&cc);
    xSemaphoreGive(camera_lock);
    return ESP_OK;
}

//-----------------------------------------------------------------------------
// Webhandler pro kořen / na sekundárním streamovacím serveru - smyčka
//-----------------------------------------------------------------------------
static esp_err_t webhandler_mjpeg_loop(httpd_req_t *http_req){
    ESP_LOGI(TAG, "HTTP GET: :81/");

    int frame_counter = 0, quality = 80; // Počítadlo snímků pro statistiku
    uint64_t mjpeg_bytes = 0;

    // Zjistíme, jestli URL obsahuje argumenty ?quality=XXX
    int qlen = httpd_req_get_url_query_len(http_req);
    if (qlen > 0) {
        char qbuf[128];
        if (qlen < (int)sizeof(qbuf) && httpd_req_get_url_query_str(http_req, qbuf, sizeof(qbuf)) == ESP_OK) {
            char val[16];
            if (httpd_query_key_value(qbuf, "quality", val, sizeof(val)) == ESP_OK) {
                int v = atoi(val);
                if (v >= 1 && v <= 100) quality = v;
            }
        }
    }

    ESP_LOGI(TAG, "HTTP URL argument quality: %d", quality);

    // Úvodní inicializace kamery, JPEG kodéru a práce se semaforem je stejná
    // jako při pořizování snímku, takže to už znovu komentovat nebudeme
    if (xSemaphoreTake(camera_lock, 0) != pdTRUE) {
        httpd_resp_set_status(http_req, "503 Service Unavailable");
        httpd_resp_set_type(http_req, "text/plain");
        httpd_resp_sendstr(http_req, "Kamera je zaneprázdněna\n");
        return ESP_OK;
    }

    CameraContext cc;
    if (camera_ctx_begin(&cc) != ESP_OK) {
        xSemaphoreGive(camera_lock);
        httpd_resp_set_status(http_req, "500 Internal Server Error");
        httpd_resp_sendstr(http_req, "Init kamery selhal\n");
        return ESP_OK;
    }

    JpegCtx jc;
    if (jpeg_ctx_begin(&jc, cc.width, cc.height, quality) != ESP_OK) {
        camera_ctx_end(&cc);
        xSemaphoreGive(camera_lock);
        httpd_resp_set_status(http_req, "500 Internal Server Error");
        httpd_resp_sendstr(http_req, "JPEG init failed\n");
        return ESP_OK;
    }

    // HTTP hlavičky pro proud JPEG obrázků oddělených slovem "frame"
    httpd_resp_set_type(http_req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(http_req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(http_req, "Pragma", "no-cache");
    httpd_resp_set_hdr(http_req, "Access-Control-Allow-Origin", "*");

    // Nekonečná streamovací smyčka. Vyskočíme z ní leda v případě, že druhá strana/klient uzavře HTTP spojení
    while (true) {
        
        // Získáme snímek z kamery. Pokud se to nepodaří, chvíli počkáme, zahodíme zbytek smyčky
        // a vracíme se na začátek pro další pokus
        int64_t t = esp_timer_get_time();
        if (camera_ctx_get_frame(&cc) != ESP_OK){
            camera_ctx_requeue(&cc);
            vTaskDelay(1);
            continue;
        };
        ESP_LOGI(TAG, "Získání RGB565 snímku %d trvalo %" PRId64 " us", frame_counter, esp_timer_get_time() - t);

        // Vracíme cyklický buffer ovladači kamery a dále už budeme pracovat jen s RGB565 framebufferem
        camera_ctx_requeue(&cc);

        // GFX operace jsou podobné jako u pořízení snímku, jen s odlišným textem s počítadlem
        t = esp_timer_get_time();
        gfx_draw_rect(&cc, 10,10, cc.width-20, cc.height-20, 255,255,255, 5);
        gfx_draw_rect(&cc, 15,15, cc.width-30, cc.height-30, 0, 0, 0, 5);
        gfx_draw_rect(&cc, 20,20, cc.width-40, cc.height-40, 255, 255, 255, 5);
        int tw = gfx_get_text_width(&Roboto60, "ESP32P4 snímek 00000");
        int x = (int)cc.width/2 - tw/2;
        char msg[100];
        sprintf(msg, "ESP32P4 snímek %d", frame_counter);
        gfx_print(&cc, x+5, 55, &Roboto60, msg, 0,0,0);
        gfx_print(&cc, x, 50, &Roboto60, msg, 255,255,255);
        sprintf(msg, "Rozměry: %ux%u px\nJPEG komprese: %d %%\nMJPEG: %" PRIu64 " B", (uint)cc.width, (uint)cc.height, quality, mjpeg_bytes);
        gfx_print(&cc, 52, cc.height - 148, &Roboto30, msg, 0,0,0);
        gfx_print(&cc, 50, cc.height - 150, &Roboto30, msg, 255,255,255);
        ESP_LOGI(TAG, "GFX operace: %" PRId64 " us", esp_timer_get_time() - t);

        // Také kódování do JPEG zůstává stejné. Pokud se ale nepodaří, chvíli počkáme
        // poté zahodíme zbytek smyčky a vracíme se na začátek 
        uint8_t *jpg; size_t jpg_len;
        t = esp_timer_get_time();
        if (jpeg_ctx_encode(&jc, cc.frame_buffer_rgb565, &jpg, &jpg_len) != ESP_OK || !jpg_len) {
            vTaskDelay(1);
            continue;
        }
        ESP_LOGI(TAG, "JPEG kódování: %" PRId64 " us", esp_timer_get_time() - t);

        mjpeg_bytes += jpg_len;

        // Připravíme HTTP hlavičky s nestandardním úvodníkem "frame",
        // proto vše děláme surově v textu
        // Už na začátku spojení jsme prohlížeči řekli, že právě "frame" bude oddělovat jednotlivé snímky
        // Mohlo by to být ale cokoliv jiného. Třeba "BOBIK"
        t = esp_timer_get_time();
        char hdr[128];
        int hdr_len = snprintf(hdr, sizeof(hdr),
            "--frame\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: %u\r\n"
            "\r\n",
            (unsigned)jpg_len);

        // Odešleme HTTP hlavičku snímku, data snímku a dvouřádkové zakončení
        // V případě chyby vyskočíme ze smyčky, protože to nejspíše znamená,
        // že druhá strana (webový prohlížeč) ukončila streamování MJPEG
        if (httpd_resp_send_chunk(http_req, hdr, hdr_len) != ESP_OK) break;
        if (httpd_resp_send_chunk(http_req, (const char*)jpg, jpg_len) != ESP_OK) break;
        if (httpd_resp_send_chunk(http_req, "\r\n", 2) != ESP_OK) break;
        ESP_LOGI(TAG, "HTTP přenos: %" PRId64 " us\r\n", esp_timer_get_time() - t);

        // Navýšíme počítadlo snímků
        frame_counter++;
        // Počkáme 10 ms, čímž stream zpomalíme a pomůže i všem bufferům po cestě
        // Záleží na testování, jaké reálné FPS nakonec zvládneme
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Vyskočili jsme z nějakého důvodu ze smyčky, a tak po sobě uklidíme a uvolníme semafor kamery
    ESP_LOGI(TAG, "Uvolňuji prostředky po ukončení MJPEG streamu");
    httpd_resp_send(http_req, NULL, 0);
    jpeg_ctx_end(&jc);
    camera_ctx_end(&cc);
    xSemaphoreGive(camera_lock);
    return ESP_OK;
}

//-----------------------------------------------------------------------------
// Funkce pro konfiguraci a start primárního kontrolního serveru 
//-----------------------------------------------------------------------------
static esp_err_t start_webserver_control(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = 80;           // Běží na standardním TCP portu 80
    config.ctrl_port = 32768;          // Nesmí kolidovat se streamovacím serverem
    config.lru_purge_enable = true;
    config.send_wait_timeout = 5;      // Timeouty spojení v sekundách
    config.recv_wait_timeout = 5;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Nemohu nastartovat primární server: %s", esp_err_to_name(err));
        return err;
    }

    // Registrace webhandlerů
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri="/capture-jpeg", .method=HTTP_GET, .handler=webhandler_capture_jpeg
    });

    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri="/", .method=HTTP_GET, .handler=webhandler_index
    });

    ESP_LOGI(TAG, "Primární HTTP server běží na TCP portu %u", config.server_port);
    return ESP_OK;
}

//-----------------------------------------------------------------------------
// Funkce pro konfiguraci a start sekundárního streamovacího serveru 
//-----------------------------------------------------------------------------
static esp_err_t start_webserver_mjpeg(void){

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.server_port = 81;           // Běží na TCP portu 81
    config.ctrl_port = 32769;          // Nesmí kolidovat s kontrolním serverem 
    config.max_open_sockets = 1;       // Jen jedno aktivní spojení
    config.lru_purge_enable = true;
    config.send_wait_timeout = 5;      // Timeouty spojení v sekundách
    config.recv_wait_timeout = 5;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Nemohu nastartovat sekundární server: %s", esp_err_to_name(err));
        return err;
    }

    // Registrace jen kořenového webhandleru,
    // protože jeho synchronní smyčka s MJPEG streamem server zcela blokuje
    // Právě proto máme dva oddělené servery, které jsou navzájem asynchronní
    httpd_register_uri_handler(server, &(httpd_uri_t){
        .uri="/", .method=HTTP_GET, .handler=webhandler_mjpeg_loop
    });

    ESP_LOGI(TAG, "Sekundární streamovací server běží na TCP portu %u", config.server_port);
    return ESP_OK;
}
