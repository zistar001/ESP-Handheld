#pragma once
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One-time init: allocate SCREENMEMORY, cachedRom, create NES machine, create video task */
esp_err_t nes_game_init(void);

/* Start a game with the given ROM path. Creates game_task on Core 1. */
void nes_game_start(const char *rom_path);

/* Stop the running game */
void nes_game_stop(void);

/* Delete the video task (must be called after game stops) */
void nes_game_cleanup_video(void);

/* Returns true if a NES game is currently running. */
bool nes_game_is_running(void);

/* Register a callback invoked when the game exits (e.g. B+START). Runs on Core 0. */
void nes_game_set_exit_callback(void (*cb)(void));

#ifdef __cplusplus
}
#endif