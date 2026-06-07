#pragma once
#include "bsp/key_driver.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One-time init: allocate NES memory, create video rendering task */
void nes_wrapper_init(void);

/* Load ROM from SD card and start emulator game task on Core 1 */
void nes_start(const char *rom_path);

/* Signal the running game to quit, block until cleanup is done */
void nes_stop(void);

/* Returns true if a NES game is currently running */
bool nes_is_running(void);

/* Forward a physical key event to the NES gamepad state.
 * Returns true if the B+START quit combo was detected. */
void nes_key_event(key_id_t key, bool pressed);

#ifdef __cplusplus
}
#endif
