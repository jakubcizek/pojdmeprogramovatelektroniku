#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

// Úloha s nekonečnou smyčkou,
// kterou spustíme na obou jádrech
static void task_hello_world(void *arg) {
    const int core = (int)(intptr_t)arg;
    while (1) {
        printf("Ahoj světe z jádra %d\n", core);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Hlavní funkce programu
void app_main(void){

    // Zjistíme informace o čipu a vypíšeme je
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    uint8_t cores = chip_info.cores;
    uint16_t major = chip_info.revision / 100;
    uint16_t minor = chip_info.revision % 100;
    printf("ESP32-P4, revize: %d.%d, jádra: %dx\n", major, minor, cores);

    // Zjistíme velikost flash paměti a vypíšeme ji
    // Správnou velikost RAM a typ čipu máme v sdkconfig.defaults
    uint32_t flash;
    esp_flash_get_size(NULL, &flash);
    printf("%" PRIu32 "MB %s flash\n", 
        flash / (uint32_t)(1024 * 1024), 
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "vnitřní" : "externí"
    );

    // Zjistíme velikst volné paměti RAM a vypíšeme ji
    // V sdkconfig.defaults jsme aktivovali PSRAM, takže dle varianty ESP32-P4 čipu
    // by to mělo být okolo 16, nebo 32 MB. Adresní prostor interní a externí RAM se sčítá
    // Interní RAM se při aktivní PSRAM preferuje pro kešování. Je totiž mnohem rychlejší
    printf("Volná RAM: %" PRIu32 " B\n", esp_get_minimum_free_heap_size());
    printf("Spouštím dvě úlohy. Každou na jednom na jádře:\n\n");

    // Spouštíme úlohu task_hello_world na jádře 0 
    xTaskCreatePinnedToCore(task_hello_world, "core0", 2048, (void*)0, 5, NULL, 0);
    // Spouštíme úlohu task_hello_world na jádře 1
    xTaskCreatePinnedToCore(task_hello_world, "core1", 2048, (void*)1, 5, NULL, 1);
}
