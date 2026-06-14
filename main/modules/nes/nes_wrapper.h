#pragma once
#include "bsp/key_driver.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One-time init: allocate NES memory, create video rendering task */
void nes_wrapper_init(void);

/* Poll: must be called from key handler. Handles game-exit cleanup
 * (LVGL-safe) when the game has ended. No-op when game is running. */
bool nes_wrapper_check_exit(void);

/* Load ROM from SD card and start emulator game task on Core 1 */
void nes_start(const char *rom_path);

/* Signal the running game to quit, block until cleanup is done */
void nes_stop(void);

/* Returns true if a NES game is currently running */
bool nes_is_running(void);

/* Forward a physical key event to the NES gamepad state. */
void nes_key_event(key_id_t key, bool pressed);

#ifdef __cplusplus
}
#endif
