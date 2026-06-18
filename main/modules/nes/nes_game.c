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
#include "esp_task_wdt.h"
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
static volatile bool s_abort = false;   /* stop requested during loading */
static TaskHandle_t s_vid_task = NULL;
static TaskHandle_t s_game_task = NULL;
static SemaphoreHandle_t s_game_done_sem = NULL;

/* Audio */
static int16_t *s_audio_buf = NULL;
static void (*s_exit_callback)(void) = NULL;
static TimerHandle_t s_audio_timer = NULL;

/* Shadow framebuffer in PSRAM — allows emulation + SPI transfer to overlap */
static uint8_t *s_shadow = NULL;

/* ================================================================
 * Display geometry — 240x280 LCD, NES is 256x240
 * Crop 8 pixels from each side, center vertically with 20px margin
 * (Vertical centering computed in video_task: voff = (280-240)/2 = 20)
 * ================================================================ */
#define CROP_X      ((NES_SCREEN_WIDTH - ST7789_WIDTH) / 2)   /* 8 */
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
        i2s_channel_write(tx, s_audio_buf, n * sizeof(int16_t), &written, pdMS_TO_TICKS(10));
        if (written == 0) break;  /* I2S 忙则跳过本帧，不阻塞定时器 */
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
    static uint32_t frame_cnt = 0;
    /* Batch buffer: 8 lines × 240px in BSS (internal SRAM, DMA-safe) */
    #define BATCH_LINES 8
    static uint16_t batch_buf[BATCH_LINES][LINE_PIXELS];

    /* NES 256x240 → crop 8px each side, voff 20 for vertical center on 280 */
    const int voff = (ST7789_HEIGHT - NES_SCREEN_HEIGHT) / 2;   /* 20 */
    const int crop = (NES_SCREEN_WIDTH - ST7789_WIDTH) / 2;      /* 8 */

    while (1) {
        xTaskNotifyWait(0, ULONG_MAX, NULL, portMAX_DELAY);
        ESP_LOGI("NES_VID", "=== DBG vid wake: s_running=%d s_shadow=%p exit_cb=%p ===",
                 s_running, (void*)s_shadow, (void*)s_exit_callback);
        if (!s_running || !s_shadow) {
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
        /* Batch render: convert palette then transfer 8 lines at once.
         * Reduces SPI transactions from 240→30, cutting command overhead ~10×. */
        for (int y = 0; y < NES_SCREEN_HEIGHT; y += BATCH_LINES) {
            int batch_h = (y + BATCH_LINES <= NES_SCREEN_HEIGHT) ? BATCH_LINES : (NES_SCREEN_HEIGHT - y);
            for (int r = 0; r < batch_h; r++) {
                uint8_t *src = s_shadow + (y + r) * NES_SCREEN_WIDTH + crop;
                for (int x = 0; x < LINE_PIXELS; x++) {
                    uint16_t c = nes_palette_256[src[x]];
                    batch_buf[r][x] = (c >> 8) | (c << 8);
                }
            }
            esp_lcd_panel_draw_bitmap(panel, 0, voff + y,
                                      LINE_PIXELS, voff + y + batch_h, (uint16_t *)batch_buf);
        }
        st7789_spi_unlock();

        /* Signal game_task: frame transfer complete */
        if (s_game_task) {
            xTaskNotify(s_game_task, 0, eSetValueWithOverwrite);
        }

        if (++frame_cnt % 120 == 0) {
            ESP_LOGI("NES_VID", "frame %lu DMA free=%zu", frame_cnt,
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
    bool wdt_added = false;  /* track WDT subscription for safe cleanup */

    ESP_LOGI(TAG, "=== DBG game_task ENTERED: %s ===", rom_path);
    ESP_LOGI(TAG, "=== DBG s_abort=%d s_game_task=%p s_vid_task=%p ===",
             s_abort, (void*)s_game_task, (void*)s_vid_task);

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

    /* Check: did someone request stop during ROM loading? */
    if (s_abort) {
        ESP_LOGW(TAG, "Stop requested during load, aborting");
        goto done;
    }

    s_running = true;
    /* Subscribe this task to the watchdog (IDF v5.x new API) */
    if (esp_task_wdt_add(NULL) == ESP_OK) {
        wdt_added = true;
    }
    /* Start audio timer at NES refresh rate */
    if (s_audio_timer) xTimerStart(s_audio_timer, 0);
    ESP_LOGI(TAG, "Running...");

    const int32_t frame_us = 1000000 / NES_REFRESH_RATE;  /* 16667us */
    uint32_t frame_count = 0;
    uint32_t t_last_log = esp_timer_get_time();
    int64_t max_frame_us = 0;
    while (s_running) {
        esp_task_wdt_reset();

        /* 1. Emulate NES frame → SCREENMEMORY */
        uint32_t t0 = esp_timer_get_time();
        nes_renderframe(true);

        /* 2. Snapshot to shadow buffer (PSRAM) + kick SPI transfer.
         *    Fire-and-forget: don't wait for SPI to finish.
         *    Video task reads from shadow while we render next frame to SCREENMEMORY. */
        memcpy(s_shadow, SCREENMEMORY, NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT);
        uint32_t t_copy = esp_timer_get_time();
        xTaskNotify(s_vid_task, 0, eSetValueWithOverwrite);

        /* 3. Frame pace to 60fps — spin-wait any remaining time */
        int32_t emu_us = (int32_t)(t_copy - t0);
        if (emu_us > max_frame_us) max_frame_us = emu_us;
        int32_t remain = frame_us - emu_us;
        if (remain > 11000) {
            vTaskDelay(pdMS_TO_TICKS((remain - 10000) / 1000));
        }
        while ((int32_t)(esp_timer_get_time() - t0) < frame_us) {
            asm volatile("nop");
        }

        /* 4. Log FPS every 5s */
        frame_count++;
        uint32_t t_now = esp_timer_get_time();
        if (t_now - t_last_log >= 5000000) {
            float fps = (float)frame_count * 1000000.0f / (float)(t_now - t_last_log);
            ESP_LOGI(TAG, "FPS=%.1f emu=%lu max=%lld",
                     fps, (unsigned long)(emu_us),
                     (long long)max_frame_us);
            frame_count = 0;
            t_last_log = t_now;
            max_frame_us = 0;
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
    /* Unsubscribe from task WDT before self-deleting — only if we
     * actually subscribed (ROM loading may have failed before that). */
    if (wdt_added) {
        esp_task_wdt_delete(NULL);
    }
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
        NESmachine->rominfo = heap_caps_malloc(sizeof(rominfo_t), MALLOC_CAP_8BIT);
        if (!NESmachine->rominfo) return ESP_ERR_NO_MEM;
        memset(NESmachine->rominfo, 0, sizeof(rominfo_t));
    }

    if (!s_shadow) {
        s_shadow = heap_caps_malloc(NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!s_audio_buf) s_audio_buf = heap_caps_malloc(DEFAULT_FRAGSIZE * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

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

    ESP_LOGI(TAG, "Init OK: SCREEN=%p ROM=%p DMA free=%zu",
             SCREENMEMORY, cachedRom, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return ESP_OK;
}

void nes_game_start(const char *rom_path) {
    if (s_running) {
        ESP_LOGW(TAG, "Game already running");
        return;
    }
    if (!rom_path) return;
    /* Reset semaphore: consume any stale signal from previous game exit */
    if (s_game_done_sem) {
        xSemaphoreTake(s_game_done_sem, 0);
    }
    /* Create game task on Core 1 */
    xTaskCreatePinnedToCore(game_task, "nes_game", 8192,
                            (void *)rom_path, 5, &s_game_task, 1);
}

void nes_game_stop(void) {
    if (!s_running && !s_game_task) return;
    s_abort = true;
    s_running = false;
    /* Wait for game_task to finish cleanup before returning (prevents
     * race where game_task sets s_running=true after we've switched to LVGL) */
    if (s_game_done_sem) {
        xSemaphoreTake(s_game_done_sem, pdMS_TO_TICKS(3000));
    }
    s_abort = false;
}

void nes_game_cleanup_video(void) {
    if (s_vid_task) {
        vTaskDelete(s_vid_task);
        s_vid_task = NULL;
    }
}

bool nes_game_is_running(void) {
    return s_running;
}

void nes_game_set_exit_callback(void (*cb)(void)) {
    s_exit_callback = cb;
}