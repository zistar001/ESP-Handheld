#!/usr/bin/env python3
"""Convert weather PNG to LVGL RGB565+Alpha C arrays for 96x96 and 20x20."""

from PIL import Image
import os

ICONS = ['sunny','cloudy','partly_cloudy','rainy','storm','snow','foggy','windy']
OUTPUT = r'D:\code\mimo\ESP_BSP\main\ui\components\weather_icons_data.c'
HEADER = r'D:\code\mimo\ESP_BSP\main\ui\components\weather_icons_data.h'
DIR = r'D:\code\mimo\ESP_BSP\tools\icons'

def rgb565_msb(r, g, b):
    val = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return (val >> 8) & 0xFF, val & 0xFF

def gen_icon(name, w, h, suffix):
    img = Image.open(os.path.join(DIR, f'{name}.png')).convert('RGBA').resize((w, h), Image.LANCZOS)
    data = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            msb, lsb = rgb565_msb(r, g, b)
            data.extend([msb, lsb, a])
    return data, w, h

with open(HEADER, 'w') as h:
    h.write('#pragma once\n#include "lvgl.h"\n\n')
    for n in ICONS:
        h.write(f'extern const lv_img_dsc_t icon_{n};\n')
        h.write(f'extern const lv_img_dsc_t icon_{n}_sm;\n')
    h.write('\nconst lv_img_dsc_t *weather_icon_get_img(const char *desc, int size);\n')

with open(OUTPUT, 'w') as out:
    out.write('#include "weather_icons_data.h"\n#include <string.h>\n\n')
    
    for name in ICONS:
        for w, h, sfx in [(96, 96, ''), (35, 35, '_sm')]:
            data, _, _ = gen_icon(name, w, h, sfx)
            sz = len(data)
            out.write(f'static const uint8_t _{name}{sfx}_data[{sz}] = {{\n')
            for i, b in enumerate(data):
                if i % 12 == 0: out.write('  ')
                out.write(f'0x{b:02X},')
                out.write('\n' if i % 12 == 11 else ' ')
            out.write('\n};\n\n')
            out.write(f'const lv_img_dsc_t icon_{name}{sfx} = {{\n')
            out.write(f'  .header.w = {w}, .header.h = {h}, .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n')
            out.write(f'  .data_size = {sz},\n')
            out.write(f'  .data = (const uint8_t *)_{name}{sfx}_data,\n')
            out.write('};\n\n')

    out.write("""const lv_img_dsc_t *weather_icon_get_img(const char *desc, int size) {
    const lv_img_dsc_t *big = &icon_sunny, *sml = &icon_sunny_sm;
    if (!desc) { /* defaults */ }
    else if (strstr(desc, "\\xe9\\x9b\\xa8")) { big = &icon_rainy; sml = &icon_rainy_sm; }
    else if (strstr(desc, "\\xe9\\x9b\\xaa")) { big = &icon_snow; sml = &icon_snow_sm; }
    else if (strstr(desc, "\\xe9\\x9b\\xb7")) { big = &icon_storm; sml = &icon_storm_sm; }
    else if (strstr(desc, "\\xe9\\x98\\xb4")) { big = &icon_cloudy; sml = &icon_cloudy_sm; }
    else if (strstr(desc, "\\xe5\\xa4\\x9a")) { big = &icon_cloudy; sml = &icon_cloudy_sm; }
    else if (strstr(desc, "\\xe6\\x99\\xb4")) { big = &icon_sunny; sml = &icon_sunny_sm; }
    else if (strstr(desc, "\\xe4\\xba\\x91")) { big = &icon_partly_cloudy; sml = &icon_partly_cloudy_sm; }
    else if (strstr(desc, "\\xe9\\x9b\\xbe")) { big = &icon_foggy; sml = &icon_foggy_sm; }
    else if (strstr(desc, "\\xe9\\xa3\\x8e")) { big = &icon_windy; sml = &icon_windy_sm; }
    return (size <= 48) ? sml : big;
}
""")
print('Done!')
