#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NES framebuffer — 256x240 uint8_t indexed pixels */
extern uint8_t *SCREENMEMORY;

/* NES controller state */
typedef struct {
    union {
        struct {
            uint8_t JOY_A      : 1;
            uint8_t JOY_B      : 1;
            uint8_t JOY_SELECT : 1;
            uint8_t JOY_START  : 1;
            uint8_t JOY_UP     : 1;
            uint8_t JOY_DOWN   : 1;
            uint8_t JOY_LEFT   : 1;
            uint8_t JOY_RIGHT  : 1;
        };
        uint8_t KEY_VALUE;
    };
} nes_pad_key_s;

extern nes_pad_key_s gamepad_p1;
extern nes_pad_key_s gamepad_p2;

/* ROM cache buffer (PSRAM) */
extern uint8_t *cachedRom;
extern uint32_t romMaxSize;

/* Audio callback for emucore */
extern void (*audio_callback)(void *buffer, int length);

/* Port initialization */
void nes_port_init(void);
void nes_port_deinit(void);

/* Start/stop emulator */
esp_err_t nes_port_start(const char *rom_path);
void nes_port_stop(void);
bool nes_port_is_running(void);

/* Key event from main.c key_handler — returns true if B+START quit */
bool nes_port_key_state(int key, bool pressed);

/* Notify video task to render a frame */
void nes_port_notify_video(void);

/* Audio buffer management */
void nes_port_audio_callback(void *buffer, int length);

#ifdef __cplusplus
}
#endif
