#include "jpeg.h"

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "Component/JPEG";

//-----------------------------------------------------------------------------
// Funkce pro vytvoření kontextu enkodéru JPEG
//-----------------------------------------------------------------------------
esp_err_t jpeg_ctx_begin(JpegCtx *jc, uint32_t width, uint32_t height, int quality){
    // Resetujeme strukturu JPEG kontextu
    memset(jc, 0, sizeof(*jc));
    jc->width = width; jc->height = height; jc->quality = quality;
    jc->bitmap_size = width * height * 2;

    // Vytvoříme JPEG enkodér s výchozí konfigurací a sekundovým timeoutem
    jpeg_encode_engine_cfg_t engine_cfg = { 
        .timeout_ms = 1000 
    };
    esp_err_t err = jpeg_new_encoder_engine(&engine_cfg, &jc->encoder);
    if (err != ESP_OK) return err;

    // Alokace bufferu pro vstup do JPEG enkodéru
    // Buffer musí mít velikost odpovídající přinejmenším velikosti našeho vstupního obrázku
    // Vstupním obrázkem je 16bit frame buffer (RGB565), velikost proto odpovídá W * H * 2
    size_t input_buffer_size = 0;
    jpeg_encode_memory_alloc_cfg_t memory_input_cfg = { 
        .buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER 
    };
    jc->input_buffer = (uint8_t*)jpeg_alloc_encoder_mem(jc->bitmap_size, &memory_input_cfg, &input_buffer_size);
    if (!jc->input_buffer || input_buffer_size < jc->bitmap_size) {
        if (jc->input_buffer) free(jc->input_buffer); 
        jpeg_del_encoder_engine(jc->encoder); 
        ESP_LOGE(TAG, "Nedostatek volné paměti pro vytvoření vstupního bufferu");
        return ESP_ERR_NO_MEM;
    }
    jc->input_buffer_size = input_buffer_size;

    // Alokace bufferu pro výstup z JPEG enkodéru
    // Výstup z enkodéru je vždy menší než vstup, jinak by to nedávalo smysl, viďte :-)
    // Alokujeme proto o něco menší buffer odpovídající 75 % vstupního frame bufferu, ať šetříme místo
    size_t output_buffer_size = 0;
    jpeg_encode_memory_alloc_cfg_t memory_output_cfg = { 
        .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER 
    };
    jc->output_buffer = (uint8_t*)jpeg_alloc_encoder_mem((jc->bitmap_size / 2) + (jc->bitmap_size / 4), &memory_output_cfg, &output_buffer_size);
    if (!jc->output_buffer || output_buffer_size == 0) { 
        if (jc->output_buffer) free(jc->output_buffer); 
        free(jc->input_buffer); 
        jpeg_del_encoder_engine(jc->encoder); 
        ESP_LOGE(TAG, "Nedostatek volné paměti pro vytvoření výstupního bufferu");
        return ESP_ERR_NO_MEM; }
    jc->output_buffer_size = output_buffer_size;

    // Samotná konfigurace ovladače enkodéru
    // Vstupem je náš frame buffer, proto vstupní formát pixelů RGB565
    // Poté snížení na formát YUV420, viz dokumentace Espressifu a JPEG
    jc->cfg = (jpeg_encode_cfg_t){
        .src_type      = JPEG_ENCODE_IN_FORMAT_RGB565,
        .sub_sample    = JPEG_DOWN_SAMPLING_YUV420,
        .image_quality = quality,
        .width         = (int)width,
        .height        = (int)height,
    };
    return ESP_OK;
}

//-----------------------------------------------------------------------------
// Funkce pro zahájení kódování
//-----------------------------------------------------------------------------
esp_err_t jpeg_ctx_encode(JpegCtx *jc, const uint16_t *input_rgb565, uint8_t **output_jpeg, size_t *output_jpeg_size){
    memcpy(jc->input_buffer, input_rgb565, jc->bitmap_size);
    uint32_t jpg_size = 0;
    esp_err_t err = jpeg_encoder_process(
        jc->encoder, 
        &jc->cfg, 
        jc->input_buffer, 
        (uint32_t)jc->bitmap_size, 
        jc->output_buffer,
         (uint32_t)jc->output_buffer_size, 
         &jpg_size
    );
    if (err == ESP_OK) {
        *output_jpeg = jc->output_buffer;
        *output_jpeg_size = (size_t)jpg_size;
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Kódování obrázku do JPEG se nepodařilo");
    return err;
}

//-----------------------------------------------------------------------------
// Funkce pro uvolnění kontextu enkodéru a vyčištění bufferů
//-----------------------------------------------------------------------------
void jpeg_ctx_end(JpegCtx *jc){
    if (!jc) return;
    if (jc->encoder) jpeg_del_encoder_engine(jc->encoder);
    if (jc->input_buffer)  free(jc->input_buffer);
    if (jc->output_buffer)  free(jc->output_buffer);
    memset(jc, 0, sizeof(*jc));
}