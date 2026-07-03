#include "rg_adapter.h"
#include "rg_system.h"
#include "ui/display_driver.h"
#include "app/app_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include <setjmp.h>
#include <string.h>

static const char *TAG = "RG_WRAP";
static volatile bool s_exit_pending = false;
static jmp_buf s_exit_jmp;
static bool s_exit_jmp_set = false;
static volatile bool s_running = false;

/* ── Emulator entry points (from retro-core) ── */
extern void nes_main(void);
//extern void gbc_main(void);
//extern void sms_main(void);
//extern void pce_main(void);

/* ── Emulator type detection ── */
typedef enum { EMU_NES, EMU_GBC, EMU_SMS, EMU_PCE, EMU_UNKNOWN } emu_type_t;

static emu_type_t detect_emu(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return EMU_UNKNOWN;
    if (strcasecmp(ext, ".nes") == 0) return EMU_NES;
    if (strcasecmp(ext, ".gb") == 0 || strcasecmp(ext, ".gbc") == 0) return EMU_GBC;
    if (strcasecmp(ext, ".sms") == 0 || strcasecmp(ext, ".gg") == 0) return EMU_SMS;
    if (strcasecmp(ext, ".pce") == 0) return EMU_PCE;
    return EMU_UNKNOWN;
}

/* ── Override rg_system_tick_hook weak symbol ── */
void rg_system_tick_hook(int busyTime) {
    (void)busyTime;
    if (s_exit_pending && s_exit_jmp_set) {
        s_exit_pending = false;
        longjmp(s_exit_jmp, 1);
    }
}

/* ── Override weak rg_system_exit / rg_system_restart to return to BSP ── */
void rg_system_exit(void) {
    ESP_LOGI(TAG, "Retro-Go exit requested");
    if (s_exit_jmp_set) {
        longjmp(s_exit_jmp, 1);
    }
    esp_restart();
}

void rg_system_restart(void) {
    rg_system_exit();
}

/* ── Public API ── */

void rg_wrapper_init(void) {
    ESP_LOGI(TAG, "Adapter ready");
    s_running = false;
    s_exit_pending = false;
    s_exit_jmp_set = false;
}

bool rg_wrapper_check_exit(void) {
    if (!s_exit_pending) return false;
    s_exit_pending = false;
    s_running = false;
    ESP_LOGI(TAG, "Emulator exited, returning to menu");
    ui_display_set_nes_active(false);
    app_manager_return();
    return true;
}

void rg_start(const char *rom_path) {
    if (s_running) {
        ESP_LOGW(TAG, "Already running");
        return;
    }

    emu_type_t emu = detect_emu(rom_path);
    if (emu == EMU_UNKNOWN) {
        ESP_LOGE(TAG, "Unknown ROM type: %s", rom_path);
        return;
    }

    s_exit_pending = false;
    s_running = true;

    /* Suppress LVGL (same as NES) */
    ui_display_set_nes_active(true);

    /* Default sample rate for retro-go audio */
    const int sample_rate = 32000;

    /* Init retro-go system */
    rg_system_clear_panic();  /* Reset RTC panic trace */
    rg_handlers_t handlers = {0};
    static rg_gui_option_t options[] = { RG_DIALOG_END };
    rg_app_t *app = rg_system_init(sample_rate, &handlers, options);
    /* Set ROM path so nes_main() can load it instead of showing file browser */
    if (app) {
        app->romPath = (char *)rom_path;
        app->bootArgs = (char *)rom_path;
    }

    /* Longjmp exit point — emulator runs from here */
    if (setjmp(s_exit_jmp) == 0) {
        s_exit_jmp_set = true;
        ESP_LOGI(TAG, "Launching: %s", rom_path);

        switch (emu) {
            case EMU_NES:  nes_main();  break;
            //case EMU_GBC:  gbc_main();  break;
            //case EMU_SMS:  sms_main();  break;
            //case EMU_PCE:  pce_main();  break;
            default: break;
        }
    }

    /* Emulator exited (via longjmp or natural return) */
    s_exit_jmp_set = false;
    ESP_LOGI(TAG, "Emulator stopped");

    /* Mark system uninitialized for next launch */
    rg_app_t *cur = rg_system_get_app();
    if (cur) cur->initialized = false;

    /* Signal exit to key_handler for LVGL restoration */
    s_exit_pending = true;
}

void rg_stop(void) {
    if (!s_running) return;
    ESP_LOGI(TAG, "Force stop");
    s_exit_pending = true;
}

bool rg_is_running(void) {
    return s_running;
}

void rg_key_event(key_id_t key, bool pressed) {
    (void)key;
    (void)pressed;
    /* Handled by retro-go's own input task */
}
