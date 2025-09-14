#ifndef JPEG_H
#define JPEG_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/jpeg_encode.h"

// Struktura kontextu enkodéru
// Kontext nastartujeme, poté můžeme kódovat snímek za snímkem
// a teprve na konci kontext uvolníme. Díky tomu šetříme prostředky
// třeba u MJPEG streamu, kde bychom jinak nastavovali kodér stále dokola
typedef struct JpegCtx {
    jpeg_encoder_handle_t encoder;
    uint8_t  *input_buffer; size_t input_buffer_size; 
    uint8_t  *output_buffer; size_t output_buffer_size;
    uint32_t  width, height;
    size_t bitmap_size;
    int quality;
    jpeg_encode_cfg_t cfg;
} JpegCtx;

// Deklarace veřejných funkcí komponenty
esp_err_t jpeg_ctx_begin(JpegCtx *jc, uint32_t width, uint32_t height, int quality);
esp_err_t jpeg_ctx_encode(JpegCtx *jc, const uint16_t *input_rgb565, uint8_t **output_jpeg, size_t *output_jpeg_size);
void jpeg_ctx_end(JpegCtx *jc);

#endif