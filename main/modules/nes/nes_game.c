#include "nes_game.h"
#include "nes_config.h"
#include "nes_port.h"
#include "nes_palette.h"
#include "emucore.h"
#include "bsp_board.h"
#include "st7789_driver.h"
#include "sd_card.h"
#include "key_driver.h"
#include "box_audio_codec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * Globals required by emucore
 * ================================================================ */
uint8_t *SCREENMEMORY = NULL;
nes_pad_key_s gamepad_p1 = {0};
nes_pad_key_s gamepad_p2 = {0};
uint8_t *cachedRom = NULL;
uint32_t romMaxSize = 0x200000;
void (*audio_callback)(void *buffer, int length) = NULL;

static const char *TAG = "NES";

/* ================================================================
 * Emulator state
 * ================================================================ */
static volatile bool s_running = false;
static TaskHandle_t s_vid_task = NULL;
static TaskHandle_t s_game_task = NULL;
static SemaphoreHandle_t s_game_done_sem = NULL;

/* Audio */
static int16_t *s_audio_buf = NULL;
static void (*s_exit_callback)(void) = NULL;
static TimerHandle_t s_audio_timer = NULL;

/* ================================================================
 * Display geometry — 240x280 LCD, NES is 256x240
 * Crop 8 pixels from each side, center vertically with 20px margin
 * ================================================================ */
#define CROP_X      ((NES_SCREEN_WIDTH - ST7789_WIDTH) / 2)   /* 8 */
#define MARGIN_Y    ((ST7789_HEIGHT - NES_SCREEN_HEIGHT) / 2) /* 20 */
#define LINE_PIXELS  ST7789_WIDTH  /* 240 */

/* ================================================================
 * Audio — called periodically to feed NES APU samples to I2S
 * ================================================================ */
static void audio_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    if (!s_running || !audio_callback || !s_audio_buf) return;

    i2s_chan_handle_t tx = box_audio_get_tx_channel();
    if (!tx) return;

    int left = DEFAULT_SAMPLERATE / NES_REFRESH_RATE;  /* 24000/60 = 400 */
    while (left > 0) {
        int n = DEFAULT_FRAGSIZE;  /* 200 */
        if (n > left) n = left;
        audio_callback(s_audio_buf, n);
        for (int i = 0; i < n; i++) {
            s_audio_buf[i] >>= 2;
        }
        size_t written = 0;
        i2s_channel_write(tx, s_audio_buf, n * sizeof(int16_t), &written, portMAX_DELAY);
        left -= written / sizeof(int16_t);
    }
}

void osd_getsoundinfo(sndinfo_t *info) {
    info->sample_rate = DEFAULT_SAMPLERATE;
    info->bps = 16;
}

/* ================================================================
 * Video task — Core 0
 * Converts indexed SCREENMEMORY -> RGB565, pushes to LCD
 * Also pumps audio each frame
 * ================================================================ */
static void video_task(void *arg) {
    (void)arg;
    static uint16_t line_buf[LINE_PIXELS];
    static uint32_t frame_cnt = 0;

    while (1) {
        xTaskNotifyWait(0, ULONG_MAX, NULL, portMAX_DELAY);
        if (!s_running || !SCREENMEMORY) {
            if (s_exit_callback) {
                void (*cb)(void) = s_exit_callback;
                s_exit_callback = NULL;
                cb();
            }
            continue;
        }


        esp_lcd_panel_handle_t panel = st7789_get_panel();
        if (!panel) continue;

        st7789_spi_lock();
        for (int y = 0; y < NES_SCREEN_HEIGHT; y++) {
            uint8_t *src = SCREENMEMORY + y * NES_SCREEN_WIDTH + CROP_X;
            for (int x = 0; x < LINE_PIXELS; x++) {
                uint16_t c = nes_palette_256[src[x]];
                line_buf[x] = ((c & 0xFF) << 8) | ((c >> 8) & 0xFF);
            }
            esp_lcd_panel_draw_bitmap(panel, 0, MARGIN_Y + y,
                                      LINE_PIXELS, MARGIN_Y + y + 1, line_buf);
        }
        st7789_spi_unlock();

        if (++frame_cnt % 120 == 0) {
            ESP_LOGI("NES_VID", "frame %lu DMA=%lu", frame_cnt,
                     heap_caps_get_free_size(MALLOC_CAP_DMA));
        }


    }
}

/* ================================================================
 * ROM loading
 * ================================================================ */
static int load_rom_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { ESP_LOGE(TAG, "Cannot open %s", path); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz > (long)romMaxSize) {
        ESP_LOGW(TAG, "ROM too large (%ld), truncating to %lu", sz, romMaxSize);
        sz = romMaxSize;
    }
    memset(cachedRom, 0, romMaxSize);
    uint32_t off = 0;
    uint8_t buf[4096];
    while (off < (uint32_t)sz) {
        size_t want = ((sz - off) > 4096) ? 4096 : (sz - off);
        size_t rd = fread(buf, 1, want, f);
        if (rd == 0) break;
        memcpy(cachedRom + off, buf, rd);
        off += rd;
    }
    fclose(f);
    ESP_LOGI(TAG, "Loaded %lu bytes from %s", off, path);
    return (int)off;
}

/* ================================================================
 * Game loop — runs on Core 1
 * ================================================================ */
static void game_task(void *arg) {
    const char *rom_path = (const char *)arg;
    int quit_cd = 0;

    ESP_LOGI(TAG, "=== GAME: %s ===", rom_path);

    box_audio_init_rate(DEFAULT_SAMPLERATE);
    box_audio_set_volume(80);

    if (load_rom_file(rom_path) < 0) goto done;

    uint8_t *romdata = cachedRom;
    memset(NESmachine->rominfo, 0, sizeof(rominfo_t));

    if (rom_getheader(&romdata, NESmachine->rominfo)) {
        ESP_LOGE(TAG, "Bad ROM header"); goto done;
    }
    ESP_LOGI(TAG, "Mapper=%d PRG=%d CHR=%d",
             NESmachine->rominfo->mapper_number,
             NESmachine->rominfo->rom_banks,
             NESmachine->rominfo->vrom_banks);

    if (!mmc_peek(NESmachine->rominfo->mapper_number)) {
        ESP_LOGE(TAG, "Unsupported mapper %d", NESmachine->rominfo->mapper_number);
        goto done;
    }

    rom_allocsram(&NESmachine->rominfo);
    rom_loadtrainer(&romdata, NESmachine->rominfo);
    if (rom_loadrom(&romdata, NESmachine->rominfo)) {
        ESP_LOGE(TAG, "ROM load failed"); goto done;
    }

    if (NESmachine->rominfo->sram) {
        NESmachine->cpu->mem_page[6] = NESmachine->rominfo->sram;
        NESmachine->cpu->mem_page[7] = NESmachine->rominfo->sram + 0x1000;
    }

    NESmachine->mmc = mmc_create(NESmachine->rominfo);
    if (!NESmachine->mmc) {
        ESP_LOGE(TAG, "Mapper create failed"); goto done;
    }

    if (NESmachine->rominfo->vram)
        NESmachine->ppu->vram_present = true;

    apu_setext(NESmachine->apu, NESmachine->mmc->intf->sound_ext);
    audio_callback = NESmachine->apu->process;

    build_address_handlers(NESmachine);
    nes_setcontext(NESmachine);
    nes_reset();

    s_running = true;
    /* Start audio timer at NES refresh rate */
    if (s_audio_timer) xTimerStart(s_audio_timer, 0);
    /* Reset video frame counter — written by video_task */
    ESP_LOGI(TAG, "Running...");

    while (s_running) {
        uint32_t t0 = esp_timer_get_time();
        nes_renderframe(true);
        xTaskNotify(s_vid_task, 0, eSetValueWithOverwrite);
        /* Precise 60fps frame limiter: vTaskDelay whole ms, spin-wait remainder us */
        int32_t t_elapsed = (int32_t)(esp_timer_get_time() - t0);
        int32_t remain_us = (1000000 / NES_REFRESH_RATE) - t_elapsed;
        if (remain_us > 0) {
            int32_t coarse_ms = (remain_us - 1000) / 1000;  /* leave ~1ms for spin */
            if (coarse_ms > 0) vTaskDelay(coarse_ms);
            while ((int32_t)(esp_timer_get_time() - t0) < (1000000 / NES_REFRESH_RATE)) { }
        }

        /* Check quit combo: B + START held for 3 frames */
        if (gamepad_p1.JOY_B && gamepad_p1.JOY_START) {
            if (++quit_cd >= 3) { s_running = false; }
        } else {
            quit_cd = 0;
        }
    }

done:
    if (s_audio_timer) xTimerStop(s_audio_timer, 0);
    audio_callback = NULL;
    if (NESmachine && NESmachine->rominfo) {
        if (NESmachine->rominfo->sram) { free(NESmachine->rominfo->sram); NESmachine->rominfo->sram = NULL; }
        if (NESmachine->rominfo->vram) { free(NESmachine->rominfo->vram); NESmachine->rominfo->vram = NULL; }
        NESmachine->rominfo->rom = NULL;
        NESmachine->rominfo->vrom = NULL;
    }
    if (NESmachine) NESmachine->mmc = NULL;
    s_running = false;
    gamepad_p1.KEY_VALUE = 0;
    gamepad_p2.KEY_VALUE = 0;
    s_game_task = NULL;

    xSemaphoreGive(s_game_done_sem);

    ESP_LOGI(TAG, "Game over");
    xTaskNotify(s_vid_task, 0, eSetValueWithOverwrite);
    vTaskDelete(NULL);
}

/* ================================================================
 * Public API
 * ================================================================ */
esp_err_t nes_game_init(void) {
    if (!SCREENMEMORY) {
        SCREENMEMORY = heap_caps_malloc(NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!cachedRom) {
        cachedRom = heap_caps_malloc(romMaxSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!SCREENMEMORY || !cachedRom) {
        ESP_LOGE(TAG, "Alloc failed: SCREEN=%p ROM=%p", SCREENMEMORY, cachedRom);
        return ESP_ERR_NO_MEM;
    }

    if (!NESmachine) {
        NESmachine = nes_create();
        if (!NESmachine) { ESP_LOGE(TAG, "nes_create failed"); return ESP_FAIL; }
        NESmachine->rominfo = heap_caps_malloc(sizeof(rominfo_t), MALLOC_CAP_DMA);
        if (!NESmachine->rominfo) return ESP_ERR_NO_MEM;
        memset(NESmachine->rominfo, 0, sizeof(rominfo_t));
    }

    if (!s_audio_buf) s_audio_buf = heap_caps_malloc(DEFAULT_FRAGSIZE * sizeof(int16_t), MALLOC_CAP_SPIRAM);

    if (!s_game_done_sem) s_game_done_sem = xSemaphoreCreateBinary();

    /* Create audio timer (60 Hz, auto-reload) — started when game runs */
    if (!s_audio_timer) {
        s_audio_timer = xTimerCreate("nes_audio", pdMS_TO_TICKS(1000 / NES_REFRESH_RATE),
                                     pdTRUE, NULL, audio_timer_cb);
    }

    if (!s_vid_task) {

        /* Video task on Core 0 */
        xTaskCreatePinnedToCore(video_task, "nes_vid", 4096, NULL, 1, &s_vid_task, 0);
    }

    ESP_LOGI(TAG, "Init OK: SCREEN=%p ROM=%p DMA free=%lu",
             SCREENMEMORY, cachedRom, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return ESP_OK;
}

void nes_game_start(const char *rom_path) {
    if (s_running) {
        ESP_LOGW(TAG, "Game already running");
        return;
    }
    if (!rom_path) return;
    /* Create game task on Core 1 */
    xTaskCreatePinnedToCore(game_task, "nes_game", 8192,
                            (void *)rom_path, 5, &s_game_task, 1);
}

void nes_game_stop(void) {
    if (!s_running) return;
    s_running = false;
    /* Wait for game task to finish cleanup (timeout 2s) */
    if (s_game_done_sem) {
        xSemaphoreTake(s_game_done_sem, pdMS_TO_TICKS(2000));
    }
}

bool nes_game_is_running(void) {
    return s_running;
}

void nes_game_set_exit_callback(void (*cb)(void)) {
    s_exit_callback = cb;
}
