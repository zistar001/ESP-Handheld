/**
 * retro-go-firmware — entry point for ESP-BSP Handheld standalone retro-go
 */
#include "rg_system.h"
#include "shared.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include "drivers/display/ili9341.h"  /* lcd_set_window, lcd_get_buffer, lcd_send_buffer */

/* ── Forward declarations ── */
void launcher_main(void);
void retro_loop(void);
void event_handler(int event, void *arg);
void options_handler(rg_gui_option_t *dest);
void about_handler(rg_gui_option_t *dest);

/* ── Global app pointer ── */
rg_app_t *app;

/* ================================================================
 * Override: rg_system_exit — return to launcher (stay on ota_0)
 * (START+B combo handles return to ESP_BSP / factory partition)
 * ================================================================ */
void rg_system_exit(void)
{
    RG_LOGI("Clearing boot config, returning to launcher...");
    char bootpath[256];
    snprintf(bootpath, sizeof(bootpath), "%s/boot.json", RG_BASE_PATH_CONFIG);
    unlink(bootpath);
    rg_settings_set_string(NS_BOOT, "BootName", NULL);
    rg_settings_set_string(NS_BOOT, "BootArgs", NULL);
    rg_settings_set_number(NS_BOOT, "BootFlags", 0);
    rg_settings_commit();
    rg_task_delay(2000);
    /* Stay on ota_0 — next boot reads empty config → launcher_main() */
    esp_restart();
    while (1);
}

void rg_system_restart(void)
{
    RG_LOGI("Restarting...");
    rg_settings_commit();
    rg_task_delay(1500);
    esp_restart();
    while (1);
}

/* ================================================================
 * launcher_main
 * ================================================================ */
void launcher_main(void)
{
    const rg_handlers_t handlers = {
        .event   = &event_handler,
        .options = &options_handler,
        .about   = &about_handler,
    };

    app = rg_system_reinit(32000, &handlers, NULL);
    app->configNs = "launcher";
    app->isLauncher = true;

    if (!rg_storage_ready()) {
        rg_display_clear(C_SKY_BLUE);
        rg_gui_alert("SD Card Error",
            "Storage mount failed.\nMake sure the card is FAT32.");
        return;
    }

    rg_storage_mkdir(RG_BASE_PATH_CACHE);
    rg_storage_mkdir(RG_BASE_PATH_CONFIG);
    heap_caps_malloc_extmem_enable(1024);
    retro_loop();
}

/* ================================================================
 * app_main
 * ================================================================ */
void app_main(void)
{
    /* Route malloc to PSRAM — emulator cores use malloc() for ROM data */
    heap_caps_malloc_extmem_enable(1024);

    app = rg_system_init(AUDIO_SAMPLE_RATE, NULL, NULL);

    RG_LOGI("configNs=%s  bootArgs=%s",
            app->configNs ?: "(null)", app->bootArgs ?: "(null)");

    if      (strcmp(app->configNs, "gbc") == 0 || strcmp(app->configNs, "gb") == 0)  gbc_main();
    else if (strcmp(app->configNs, "nes") == 0)  nes_main();
    else if (strcmp(app->configNs, "pce") == 0)  pce_main();
    else if (strcmp(app->configNs, "sms") == 0 || strcmp(app->configNs, "gg") == 0 ||
             strcmp(app->configNs, "col") == 0)  sms_main();
    else if (strcmp(app->configNs, "gw") == 0)   gw_main();
    else if (strcmp(app->configNs, "snes") == 0) snes_main();
    else if (strcmp(app->configNs, "lnx") == 0)  lynx_main();
    else launcher_main();

    RG_PANIC("Never reached");
}
