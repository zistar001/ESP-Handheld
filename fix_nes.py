# -*- coding: utf-8 -*-
content = open('main/modules/nes/nes_game.c', 'r', encoding='utf-8').read()

# 1. SCREENMEMORY back to PSRAM
content = content.replace(
    'SCREENMEMORY = heap_caps_malloc(NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT,\n                                         MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);',
    'SCREENMEMORY = heap_caps_malloc(NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT,\n                                         MALLOC_CAP_SPIRAM);'
)

# 2. MARGIN_Y 0 -> 20
content = content.replace('#define MARGIN_Y    0', '#define MARGIN_Y    20')

# 3. Remove screen_buf allocation
content = content.replace(
    '    /* Full screen buffer with black borders \xe2\x80\x94 240*280*2 = 134KB */\n    uint16_t *screen_buf = heap_caps_malloc(ST7789_WIDTH * ST7789_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);\n    if (!screen_buf) { ESP_LOGE("NES_VID", "screen buf alloc failed"); vTaskDelete(NULL); }',
    ''
)

# 4. Remove blank lines before while(1)
content = content.replace('}\n\n\n    while (1)', '}\n\n    while (1)')

# 5. Remove memset
content = content.replace(
    '        /* Black out entire screen */\n        memset(screen_buf, 0, ST7789_WIDTH * ST7789_HEIGHT * sizeof(uint16_t));\n\n',
    ''
)

# 6. Remove voff / center-compositing comment
content = content.replace(
    '        /* Build NES 240x240 frame into center of screen_buf */\n        int voff = (ST7789_HEIGHT - NES_SCREEN_HEIGHT) / 2; /* center = 20 */',
    ''
)

# 7. screen_buf -> frame_buf in pixel copy
content = content.replace(
    'uint16_t *dst = screen_buf + (y + voff) * LINE_PIXELS;',
    'uint16_t *dst = frame_buf + (y + MARGIN_Y) * LINE_PIXELS;'
)

# 8. Remove SPI lock wrapper, update draw_bitmap to use frame_buf
content = content.replace(
    '        /* Send entire screen in ONE SPI transaction */\n        st7789_spi_lock();\n        esp_lcd_panel_draw_bitmap(panel, 0, 0,\n                                  ST7789_WIDTH, ST7789_HEIGHT, screen_buf);\n        st7789_spi_unlock();',
    '        esp_lcd_panel_draw_bitmap(panel, 0, MARGIN_Y, ST7789_WIDTH, NES_SCREEN_HEIGHT + MARGIN_Y, frame_buf);'
)

# 9. Resize frame_buf to include margin
content = content.replace(
    'uint16_t *frame_buf = heap_caps_malloc(LINE_PIXELS * NES_SCREEN_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);',
    'uint16_t *frame_buf = heap_caps_malloc(LINE_PIXELS * (NES_SCREEN_HEIGHT + MARGIN_Y) * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);'
)

open('main/modules/nes/nes_game.c', 'w', encoding='utf-8').write(content)
print('Done')
