#pragma once
#include <stdint.h>
#include <stdio.h>

/* NES emulator configuration */
#define NES_REFRESH_RATE 60
#define NES_VISIBLE_HEIGHT 240
#define NES_SCREEN_WIDTH 256
#define NES_SCREEN_HEIGHT 240
#define NES_CLOCK_DIVIDER 12
#define NES_MASTER_CLOCK (236250000 / 11)
#define NES_SCANLINE_CYCLES (1364.0f / NES_CLOCK_DIVIDER)
#define NES_FIQ_PERIOD (NES_MASTER_CLOCK / NES_CLOCK_DIVIDER / 60)
#define NES_RAMSIZE 0x800
#define NES_SKIP_LIMIT (NES_REFRESH_RATE / 5)
#define NES6502_NUMBANKS 16
#define NES6502_BANKSHIFT 12
#define NES6502_BANKSIZE (0x10000 / NES6502_NUMBANKS)
#define NES6502_BANKMASK (NES6502_BANKSIZE - 1)
#define MAX_MEM_HANDLERS 32
#define DEFAULT_SAMPLERATE 24000
#define DEFAULT_FRAGSIZE 200

#define HOST_LITTLE_ENDIAN
#define ZERO_LENGTH 0
#define UNUSED_NES(x) ((x) = (x))
#define NTSC

#define SCREEN_RES_HOR 240
#define SCREEN_RES_VER 280

#define SOUND_ENABLED 1

/* C++ overloads for logging (used by emucore.cpp) */
#ifdef __cplusplus
static inline void SHOW_MSG_SERIAL(uint8_t c) { printf("%c", c); }
static inline void SHOW_MSG_SERIAL(const char *s) { printf("%s", s); }
#else
#define SHOW_MSG_SERIAL(x) printf("%s", (const char*)(x))
#endif

#define STAY_HERE while(1){}
