#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_async_memcpy.h"
#include "esp_memory_utils.h"
#include "esp_cache.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define SIZE_PSRAM            (512*1024)  // 512KiB testovací blok ve SPI PSRAM
#define SIZE_RAM              (64*1024)   // 64KiB testovací blok v interni RAM na CPU
#define PSRAM_BENCHMARK_RUNS  20          // Počet opakování R/W testů

// Test primitivního kopírování bloků paměti mezi RAM a PSRAM
// První test používá smyčku klasické funkce memcpy, která ale zatěžuje procersor
// Druhý test do hry zapojuje DMA a bloky kopíruje asynchronně pomocí funkce esp_async_memcpy (měli bychom dosáhnout vyšší rychlosti) 


// Deklarace funkcí
static void psram_cpu_suite(uint8_t *ps, uint8_t *ib);           // Jednoduchý test kopírování paměti mezi RAM a PSRAM pomocí memcpy s blokováním CPU
static double mibps(size_t bytes, int64_t us);                   // Výpočet rychlost v MiB/s

static void psram_dma_suite(uint8_t *ps, uint8_t *ib);           // Složitější test, kdy kopírujeme data mezi RAM a PSRAM pomocí DMA (GDMA AXI) a esp_async_memcpy
static void wait_all(SemaphoreHandle_t sem, uint32_t count);     // Pomocné funkce pro asynchronní čekávání na asynchronní kopírování
static bool memcpy_done_cb(async_memcpy_handle_t mcp, 
async_memcpy_event_t *e, void *arg);                         // Pomocné funkce pro asynchronní čekávání na asynchronní kopírování
static void submit_window(async_memcpy_handle_t h, 
    SemaphoreHandle_t done, uint8_t *dst_base, 
    uint8_t *src_base, size_t total_size, 
    size_t chunk_size, uint32_t backlog, int dst_increments);    // Pomocné funkce pro asynchronní čekávání na asynchronní kopírování

//-----------------------------------------------------------------------------
// Hlavní funkce a začátek programu. Analogie setup() ze světa Arduino
//-----------------------------------------------------------------------------
void app_main(void){

    printf("\n\n\n");

    // Alokace 512 KiB externí SPI PSRAM
    uint8_t *psram_test_memory = (uint8_t*)heap_caps_aligned_alloc(64, SIZE_PSRAM, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    // Alokace 64 KiB v interní RAM na CPU
    uint8_t *ram_test_memory = (uint8_t*)heap_caps_aligned_alloc(64, SIZE_RAM, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    
    if (!psram_test_memory || !ram_test_memory) {
        printf("Alokace selhala\n"); 
        return; 
    }

    // Naplníme testovací bloky paměti bajty třeba s hodnotou 0xAB a 0xCD
    // Nehraje to roli, ale může se to hodit při případném ladění
    memset(psram_test_memory, 0xAB, SIZE_PSRAM);
    memset(ram_test_memory, 0xCD, SIZE_RAM);

    // Přinejmenším první test poběží i na variantách ESP32-S3 s PSRAM,
    // proto můžeme projekt přeložit i na něm a srovnat dosažené hodnoty
#if CONFIG_IDF_TARGET_ESP32P4
    printf("Target: ESP32-P4 (PSRAM 16-line @ 200 MHz)\n");
    psram_cpu_suite(psram_test_memory, ram_test_memory); // CPU memcpy
    psram_dma_suite(psram_test_memory, ram_test_memory); // DMA asycnhronní memcpy

#elif CONFIG_IDF_TARGET_ESP32S3
    printf("Target: ESP32-S3 (PSRAM 8-line @ 120 MHz)\n");
    psram_cpu_suite(ps, ib); // CPU memcpy
#endif

    heap_caps_free(ram_test_memory);
    heap_caps_free(psram_test_memory);

    printf("\n\n\n");
}

//-----------------------------------------------------------------------------
//Funkce pro výpočet MiB/s
//-----------------------------------------------------------------------------
static double mibps(size_t bytes, int64_t us){
    return (bytes * 1.0e6 / (double)us) / (1024.0*1024.0);
}

//-----------------------------------------------------------------------------
// Funkce testovací sady pomocí běžného memcpy
// Postupně plníme PSRAM blokem z RAM (test zápisu)
// Poté postupně z PSRAM přepisujeme blok v RAM (tets čtení)
// Obě smyčky se opakují pdole hodnoty v PSRAM_BENCHMARK_RUNS
//-----------------------------------------------------------------------------
static void psram_cpu_suite(uint8_t *psram, uint8_t *ram) {
    // Pro začátek to trošku rozehřejeme...
    for (size_t off = 0; off < SIZE_PSRAM; off += SIZE_RAM) memcpy(psram + off, ram, SIZE_RAM);
    for (size_t off = 0; off < SIZE_PSRAM; off += SIZE_RAM) memcpy(ram, psram + off, SIZE_RAM);

    // A teď už začínáme měřit zapisování RAM do PSRAM
    int64_t t0 = esp_timer_get_time();
    for (uint32_t r = 0; r < PSRAM_BENCHMARK_RUNS; r++){
        for (size_t off = 0; off < SIZE_PSRAM; off += SIZE_RAM) memcpy(psram + off, ram, SIZE_RAM);
    }
    int64_t t1 = esp_timer_get_time();

    // Teď měříme čtení z PSRAM do RAM
    // Proměnná sink se tu plní zbytečně,
    // ale slouží k tomu, aby překladač celý blok otpimalziací nesmazal
    // jako naprostou zbytečnost. Přepisujeme totiž stále dokola ten stejný blok,
    // což dává smysl jen u benchmarku
    volatile uint32_t sink = 0;
    int64_t t2 = esp_timer_get_time();
    for (uint32_t r = 0; r < PSRAM_BENCHMARK_RUNS; r++){
        for (size_t off = 0; off < SIZE_PSRAM; off += SIZE_RAM) {
            memcpy(ram, psram + off, SIZE_RAM);
            sink += ram[0];
        }
    }
    int64_t t3 = esp_timer_get_time();

    // Máme hotovo, vypíšme statistiku přensenýych bajtů a rychlosti
    size_t bytes = (size_t)SIZE_PSRAM * PSRAM_BENCHMARK_RUNS;
    printf("\nCPU memcpy | Přeneseno %u B (%ux opakování, blok %u B)\n", (unsigned)bytes, PSRAM_BENCHMARK_RUNS, (unsigned)SIZE_RAM);
    printf("  Zápis: %.2f MiB/s\n  Čtení: %.2f MiB/s\n", mibps(bytes, t1 - t0), mibps(bytes, t3 - t2));
}

//-----------------------------------------------------------------------------
// Funkce testovací sady pomocí běžného asynchronního memcpy skrze DMA
// Postupně plníme PSRAM blokem z RAM (test zápisu)
// Poté postupně z PSRAM přepisujeme blok v RAM (tets čtení)
// Obě smyčky se opakují pdole hodnoty v PSRAM_BENCHMARK_RUNS
//-----------------------------------------------------------------------------
static void psram_dma_suite(uint8_t *psram, uint8_t *ram){
    async_memcpy_handle_t h = NULL;
    async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG();
    const uint32_t BACKLOG = 8;    
    cfg.backlog = BACKLOG;
    cfg.dma_burst_size = 64; 

    if (esp_async_memcpy_install_gdma_axi(&cfg, &h) != ESP_OK) {
        if (esp_async_memcpy_install(&cfg, &h) != ESP_OK) {
            printf("\nInstalace ovladače GDMA AXI pro asynchronní memcpy selhala!\n");
            return;
        }
    }

    SemaphoreHandle_t done = xSemaphoreCreateCounting(BACKLOG, 0);

    // Pro začátek to trošku rozehřejeme...
    submit_window(h, done, psram, ram, SIZE_PSRAM, SIZE_RAM, BACKLOG, /*dst_increments=*/1);
    submit_window(h, done, ram, psram, SIZE_PSRAM, SIZE_RAM, BACKLOG, /*dst_increments=*/0);

    // A teď už začínáme měřit zapisování RAM do PSRAM
    int64_t t0 = esp_timer_get_time();
    for (uint32_t r = 0; r < PSRAM_BENCHMARK_RUNS; r++) {
        submit_window(h, done, psram, ram, SIZE_PSRAM, SIZE_RAM, BACKLOG, 1);
    }
    int64_t t1 = esp_timer_get_time();

    // A teď měříme opět čtení z PSRAM do RAM
    volatile uint32_t sink = 0;
    int64_t t2 = esp_timer_get_time();
    for (uint32_t r = 0; r < PSRAM_BENCHMARK_RUNS; r++) {
        submit_window(h, done, ram, psram, SIZE_PSRAM, SIZE_RAM, BACKLOG, 0);
        sink += ram[0];
    }
    int64_t t3 = esp_timer_get_time();

    vSemaphoreDelete(done);
    esp_async_memcpy_uninstall(h);

    // Máme hotovo, vypíšme statistiku přensenýych bajtů a rychlosti
    size_t bytes = (size_t)SIZE_PSRAM * PSRAM_BENCHMARK_RUNS;
    printf("\nDMA memcpy | Přeneseno %u B (%ux opakování, blok %u B)\n", (unsigned)bytes, PSRAM_BENCHMARK_RUNS, (unsigned)SIZE_RAM);
    printf("  Zápis: %.2f MiB/s\n  Čtení: %.2f MiB/s\n", mibps(bytes, t1 - t0), mibps(bytes, t3 - t2));
}

//-----------------------------------------------------------------------------
// Funkce/callback, který se volá po asynchronním zápisu bloku paměti skrze DMA
//-----------------------------------------------------------------------------
static bool memcpy_done_cb(async_memcpy_handle_t mcp, async_memcpy_event_t *e, void *arg){
    SemaphoreHandle_t sem = (SemaphoreHandle_t)arg;
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(sem, &hp);
    return (hp == pdTRUE);
}

//-----------------------------------------------------------------------------
// Funkce pro čekání na zkopírování paměti
//-----------------------------------------------------------------------------
static void wait_all(SemaphoreHandle_t sem, uint32_t count){
    uint32_t i;
    for (i = 0; i < count; i++) {
        xSemaphoreTake(sem, portMAX_DELAY);
    }
}

//-----------------------------------------------------------------------------
// Funkce pro asynchronní kopríování po bezpečných blocích
//-----------------------------------------------------------------------------
static void submit_window(async_memcpy_handle_t h, SemaphoreHandle_t done,
                          uint8_t *dst_base, uint8_t *src_base,
                          size_t total_size, size_t chunk_size,
                          uint32_t backlog, int dst_increments)
{
    size_t off = 0;
    while (off < total_size) {
        uint32_t submitted = 0;
        while (submitted < backlog && off < total_size) {
            void *dst = dst_increments ? (void *)(dst_base + off) : (void *)dst_base;
            void *src = dst_increments ? (void *)(src_base + off) : (void *)src_base;
            ESP_ERROR_CHECK(esp_async_memcpy(h, dst, src, chunk_size, memcpy_done_cb, (void *)done));
            submitted++;
            off += chunk_size;
        }
        wait_all(done, submitted);
    }
}