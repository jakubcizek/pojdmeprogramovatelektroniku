#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_video_device.h"
#include "esp_video_init.h"

#include "camera.h"

static const char *TAG = "Component/Camera";

// Kontrolní proměnná, která zamezí opakování inicialziace již inicializovaného 
static bool is_mipi_csi_bus_initialised = false;

//-----------------------------------------------------------------------------
// Struktura s konfigurací sběrnice MIPI CSI
// Všimněte si, že zde nastavujeme jeji I2C sběrnici,
// která se používá pro řízení. MIPI CSI tedy používá paralelení data + I2C 
//-----------------------------------------------------------------------------
static const esp_video_init_csi_config_t csi_config = {
    .sccb_config = {
        .init_sccb = true,
        .i2c_config = {
            .port      = 0,
            .scl_pin   = 8,
            .sda_pin   = 7,
        },
        .freq = 100000,
    },
    .reset_pin = -1,
    .pwdn_pin  = -1,
};

//-----------------------------------------------------------------------------
// Struktura pro konfiguraci okruhu videa, kterou nakrmíme předochozí
// konfigurací MIPI CSI
//-----------------------------------------------------------------------------
static const esp_video_init_config_t video_config = {
    .csi = &csi_config
};

//-----------------------------------------------------------------------------
// Jednoduchá funkce pro inicialziaci MIPI CSI
// s ochranou na opakované spuštění
//-----------------------------------------------------------------------------
esp_err_t mipi_csi_bus_init(void){
    esp_err_t ret;
    if (is_mipi_csi_bus_initialised) {
        return ESP_OK;
    }
    ret = esp_video_init(&video_config);
    if(ret == ESP_OK){
        is_mipi_csi_bus_initialised = true;
    }
    return ret;
}

//-----------------------------------------------------------------------------
// Jednoduchá funkce pro uvolnění MIPI CSI
// s ochranou na opakované spuštění
//-----------------------------------------------------------------------------
esp_err_t mipi_csi_bus_deinit(void){
    esp_err_t ret = ESP_OK;
    if (!is_mipi_csi_bus_initialised) {
        return ESP_OK;
    }
    ret = esp_video_deinit();
    is_mipi_csi_bus_initialised = false;
    return ret;
}

//-----------------------------------------------------------------------------
// Funkce pro inicializaci kamery, respektive vrstvy V4L (video for linux),
// kterou Espressif používá z důvodu API kompatibility, žádný
// Linux totiž na načem ESP32-P4 totiž opravdu neběží. V4L je ale už norma
// Volba snímacícho čipu se řeší v konfigurátoru menuconfig a nikoliv v kódu
//-----------------------------------------------------------------------------
esp_err_t camera_ctx_begin(CameraContext *cc){
    memset(cc, 0, sizeof(*cc));
    // Kamera poběží v režimu streamování a nikoliv pořizování statických fotek ve vysokém rozlišení
    // Pracujeme s rozlišením do 1080p, které je limitní i pro některé multimediální periferie
    cc->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (mipi_csi_bus_init() != ESP_OK) {
        ESP_LOGE(TAG, "Nepodařilo se nastartovat MIPI CSI");
        return ESP_FAIL;
    }
    // Otevíráme spojení s V4L driverem kamery na /dev/video0
    cc->device = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (cc->device < 0) {
        ESP_LOGE(TAG, "Nepodařilo se otevřít kameru %s, číslo chyby: %d", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, errno);
        goto fail_deinit;
    }

    // Konfigurace kamery skrze její schopnosti/capabilities, což
    // je další vlastnost ovladačů V4L. Tady vás tedy odkážu na obecnou dokumentaci V4L
    // Driver od Espressifu nabízí jen subset V4L/zákaldní API
    struct v4l2_capability cap = {0};
    if (ioctl(cc->device, VIDIOC_QUERYCAP, &cap) != 0){ 
        ESP_LOGE(TAG, "Driver neumí V4L schopnost VVIDIOC_QUERYCAP (stažení snímku)"); 
        goto fail_close_deinit; 
    }

    struct v4l2_format fmt = { .type = cc->type };
    if (ioctl(cc->device, VIDIOC_G_FMT, &fmt) != 0){ 
        ESP_LOGE(TAG, "Driver neumí V4L schopnost VIDIOC_G_FMT (formát)"); 
        goto fail_close_deinit; 
    }

    // Chceme od driveru snímky ve formátu RGB565 a v rozlišení, které jsme nastavili v menuconfig, čili mimo kód
    // Podporovaná rozlišení se liší kamera od kamery podle toho, jak pro ni Espressif připravil driver
    // V sdkconfig.defaults ve výchozím stavu pracujeme s rolzišením 800x640px, k dispozici je ale i 1080p
    struct v4l2_format frame_format = {
        .type = cc->type,
        .fmt.pix.width       = fmt.fmt.pix.width,
        .fmt.pix.height      = fmt.fmt.pix.height,
        .fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565
    };
    if (ioctl(cc->device, VIDIOC_S_FMT, &frame_format) != 0){ 
        ESP_LOGE(TAG, "Nastavení V4L schopnosti VIDIOC_S_FMT na RGB565 selhalo"); 
        goto fail_close_deinit;
    }
    if (ioctl(cc->device, VIDIOC_G_FMT, &frame_format) != 0){
        ESP_LOGE(TAG, "V4L schopnost VIDIOC_G_FMT (nastavení formátu) selhala");
        goto fail_close_deinit;
    }

    // Do naší centrální struktury CameraContext načteme údaje o rozměrech snímku a velikosti pracovního RGB565 framebufferu
    cc->width = frame_format.fmt.pix.width;
    cc->height = frame_format.fmt.pix.height;
    cc->ring_buffer_line_stride = frame_format.fmt.pix.bytesperline > 0 ? frame_format.fmt.pix.bytesperline : (int)cc->width * 2;
    cc->frame_buffer_rgb565_size = (size_t)cc->width * cc->height * 2;

    // Incializace cyklického bufferu V4L, který bude mít hloubku podle makra BUFFER_COUNT z shared_types.h
    // V našem výchozím stavu to jsou 2 snímky
    // Cyklický buffer má více snímků proto, že když do prvního zapisuje nová data z kamery,
    // my můžeme z toho o krok staršího číst data do našeho pracovního framebufferu
    // V4L cyklický buffer neustále automaticky přepisuje a nám díky němu nehrozí, že čteme snímek z kamery v okamžiku,
    // kdy už jej plní novými daty a snímek by byl poškozený
    struct v4l2_requestbuffers v4l2_req = {0};
    v4l2_req.count  = RING_BUFFER_COUNT;
    v4l2_req.type   = cc->type;
    v4l2_req.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(cc->device, VIDIOC_REQBUFS, &v4l2_req) != 0){
        ESP_LOGE(TAG, "V4L schopnost VIDIOC_REQBUFS (cyklický buffer) selhala"); 
        goto fail_close_deinit;
    }
    for (int i = 0; i < RING_BUFFER_COUNT; i++) {
        struct v4l2_buffer ring_buffer_descriptor;
        memset(&ring_buffer_descriptor, 0, sizeof(ring_buffer_descriptor));
        ring_buffer_descriptor.type   = cc->type;
        ring_buffer_descriptor.memory = V4L2_MEMORY_USERPTR;
        ring_buffer_descriptor.index  = i;
        if (ioctl(cc->device, VIDIOC_QUERYBUF, &ring_buffer_descriptor) != 0){
            ESP_LOGE(TAG, "V4L schopnost VIDIOC_QUERYBUF[%d] (cyklického buffer) selhala", i);
            goto fail_free_ring_buffers_close_deinit;
        }
        cc->ring_buffer[i] = heap_caps_aligned_alloc(MEMORY_ALIGN, ring_buffer_descriptor.length, MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
        if (!cc->ring_buffer[i]){
            ESP_LOGE(TAG, "Alokace cyklického ring bufferu %d (%u B) selhala", i, (unsigned)ring_buffer_descriptor.length); 
            goto fail_free_ring_buffers_close_deinit; 
        }
        ring_buffer_descriptor.m.userptr = (unsigned long)cc->ring_buffer[i];
        cc->ring_buffer_size[i] = ring_buffer_descriptor.length;
        if (ioctl(cc->device, VIDIOC_QBUF, &ring_buffer_descriptor) != 0){
            ESP_LOGE(TAG, "V4L  schopnost VIDIOC_QBUF[%d] (cyklický buffer) selhala", i);
            goto fail_free_ring_buffers_close_deinit;
        }
    }
    // Aktivace streamování
    if (ioctl(cc->device, VIDIOC_STREAMON, &cc->type) != 0){
        ESP_LOGE(TAG, "V4L schopnost VIDIOC_STREAMON (aktivace streamu) selhala"); 
        goto fail_free_ring_buffers_close_deinit; 
    }
    // Vytvoření 16bit RGB565 pracovního framebufferu
    cc->frame_buffer_rgb565 = (uint16_t*)heap_caps_malloc(cc->frame_buffer_rgb565_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!cc->frame_buffer_rgb565){
        ESP_LOGE(TAG, "Alokace 16bit RGB565 pracovního framebufferu (%u B) selhala", (unsigned)cc->frame_buffer_rgb565_size);
        goto fail_streamoff_free_ring_buffers_close_deinit;
    }

    // Pokud jsme se dostali až sem, vrstva V4L a kamera by měla být nastartovaná,
    // cyklický buffer V4L se automaticky přepisuje a náš pracovní 16bit RGBG565 framebuffer je také připravený
    // Buffery zabírají dohromady megabajty dat, takže se vše samozřejmě odehrává v externí PSRAM,
    // která má oproti interní RAM své limity - je několikanásobně pomalejší
    return ESP_OK;

    // Sestupná chybová návěstidla, do kterých padá konfigurace výše
    // Vyčistíme prostředky, uvolníme sběrnici MIPI CSI a vrátíme chybu
fail_streamoff_free_ring_buffers_close_deinit:
    (void)ioctl(cc->device, VIDIOC_STREAMOFF, &cc->type);
fail_free_ring_buffers_close_deinit:
    for (int i = 0; i < RING_BUFFER_COUNT; i++){
        if (cc->ring_buffer[i]){
            heap_caps_free(cc->ring_buffer[i]);
        }
    }
fail_close_deinit:
    if (cc->device >= 0) close(cc->device);
fail_deinit:
    mipi_csi_bus_deinit();
    memset(cc, 0, sizeof(*cc));
    return ESP_FAIL;
}

//-----------------------------------------------------------------------------
// Funkce pro naplnění 16bit RGB565 pracovního framebufferu 
// Výsledkem je pole s jednotlivými řádky 16bit pixelů snímku
// Šlo by to možná vylepšit a zrychlit přepsáním do DMA (asynchronní memcpy)?
//-----------------------------------------------------------------------------
esp_err_t camera_ctx_get_frame(CameraContext *cc){
    struct v4l2_buffer *rbd = &cc->ring_buffer_descriptor;
    memset(rbd, 0, sizeof(*rbd));
    rbd->type   = cc->type;
    rbd->memory = V4L2_MEMORY_USERPTR;
    if (ioctl(cc->device, VIDIOC_DQBUF, rbd) != 0) return ESP_FAIL;
    const uint8_t *src_base = cc->ring_buffer[rbd->index];
    const size_t tight_row = (size_t)cc->width * 2;
    if (cc->ring_buffer_line_stride == (int)tight_row) {
        memcpy(cc->frame_buffer_rgb565, src_base, cc->frame_buffer_rgb565_size);
    } else {
        for (uint32_t y = 0; y < cc->height; ++y) {
            const uint8_t *s = src_base + (size_t)y * cc->ring_buffer_line_stride;
            uint8_t *d = (uint8_t*)cc->frame_buffer_rgb565 + (size_t)y * tight_row;
            memcpy(d, s, tight_row);
        }
    }
    return ESP_OK;
}

//-----------------------------------------------------------------------------
// Funkce pro vrácení cyklického bufferu V4L vrstvě
// Říkáme tím: Už jsme si stáhli snímek z aktuálního volného bufferu (máme 2),
// takže je volný a klidně ho zase přepiš
//-----------------------------------------------------------------------------
void camera_ctx_requeue(CameraContext *cc){
    struct v4l2_buffer *rbd = &cc->ring_buffer_descriptor;
    rbd->type     = cc->type;
    rbd->memory   = V4L2_MEMORY_USERPTR;
    rbd->m.userptr = (unsigned long)cc->ring_buffer[rbd->index];
    rbd->length    = cc->ring_buffer_size[rbd->index];
    (void)ioctl(cc->device, VIDIOC_QBUF, rbd);
}

//-----------------------------------------------------------------------------
// Funkce pro ukončení práce s kamerou
// Vypneme V4L streamování uviolníme cyklický a pracovní buffer
// a na závěr uvolníme sběrnici MIPI CSI
//-----------------------------------------------------------------------------
esp_err_t camera_ctx_end(CameraContext *cc){
    if (!cc) return ESP_FAIL;
    esp_err_t ret;
    (void)ioctl(cc->device, VIDIOC_STREAMOFF, &cc->type);
    for (int i = 0; i < RING_BUFFER_COUNT; i++) if (cc->ring_buffer[i]) heap_caps_free(cc->ring_buffer[i]);
    if (cc->frame_buffer_rgb565) heap_caps_free(cc->frame_buffer_rgb565);
    if (cc->device >= 0) close(cc->device);
    ret = mipi_csi_bus_deinit();
    memset(cc, 0, sizeof(*cc));
    return ret;
}