#ifndef CAMERA_H
#define CAMERA_H

#include "esp_err.h"
#include "linux/videodev2.h"

// Nastavení typu paměti vrstvy V4L
// Bude zarovnaná na 64 B (zarovnání je důležité pro efektivní přenosy, kešování atp,)
// a vše se bude odehrávat v našem společném adresním rozsahu interní RAM + externí RAM
// Rychlost SPI PSRAM tedy limituje celkovou rychlost
// ESP32-P4 naštěstí podprouje 16bit šířku SPI a takt až 200 MHz,
// který nastavujeme jako výchozí v sdkconfig.defaults
// PSRAM jinak nastavíte v interaktivním menuconfig
#define MEMORY_TYPE  V4L2_MEMORY_USERPTR
#define MEMORY_ALIGN 64

// Hloubka (počet snímků) cyklického ring bufferu V4L
#define RING_BUFFER_COUNT 2

// Klíčová struktura s obrazovými buffery a metadaty
// Přistupuje k ní i komponenta gfx, která má  komponentu camera v závislosti
// Nabízí se tedy rozdělení kamerového a framebufferového kontextu
typedef struct CameraContext {
    int       device;                              // Zařízení, respektive „linuxový“ file deskriptor kamery (/dev/video0)
    int       type;                                // Typ bufferu (obvykle V4L2_BUF_TYPE_VIDEO_CAPTURE)
    uint32_t  width, height;                       // Šířka a výška snímku v pixelech
    uint8_t  *ring_buffer[RING_BUFFER_COUNT];      // ukazatele na cyklické buffery spravované V4L2
    size_t    ring_buffer_size[RING_BUFFER_COUNT]; // velikost každého cyklického bufferu
    int       ring_buffer_line_stride;             // Skutečná délka řádku ve zdrojovém bufferu (bytesperline)
    struct v4l2_buffer ring_buffer_descriptor;     // poslední použitý V4L2 buffer (DQBUF/QBUF metadata)
    uint16_t *frame_buffer_rgb565;                 // vlastní pracovní framebuffer v RGB565, řádkově těsný
    size_t    frame_buffer_rgb565_size;            // velikost pracovního framebufferu v bajtech
} CameraContext;

// Deklarace veřejných funkcí komponenty
esp_err_t mipi_csi_bus_init(void);
esp_err_t mipi_csi_bus_deinit(void);
esp_err_t camera_ctx_begin(CameraContext *cc);
esp_err_t camera_ctx_end(CameraContext *cc);
esp_err_t camera_ctx_get_frame(CameraContext *cc);
void camera_ctx_requeue(CameraContext *cc);

#endif