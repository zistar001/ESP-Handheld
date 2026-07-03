#include "weather_icon.h"
#include "weather_icons_data.h"
#include <string.h>

lv_obj_t *weather_icon_create(lv_obj_t *parent, const char *desc, int size) {
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, weather_icon_get_img(desc, size));
    return img;
}

void weather_icon_update(lv_obj_t *img, const char *desc, int size) {
    if (!img || !desc) return;
    lv_img_set_src(img, weather_icon_get_img(desc, size));
}

weather_icon_t weather_icon_from_desc(const char *desc) {
    if (!desc) return WEATHER_SUNNY;
    if (strstr(desc,"\xe9\x9b\xa8")) return WEATHER_RAINY;
    if (strstr(desc,"\xe9\x9b\xaa")) return WEATHER_SNOW;
    if (strstr(desc,"\xe9\x9b\xb7")) return WEATHER_STORM;
    if (strstr(desc,"\xe9\x98\xb4")) return WEATHER_CLOUDY;
    if (strstr(desc,"\xe5\xa4\x9a")) return WEATHER_CLOUDY;
    if (strstr(desc,"\xe6\x99\xb4")) return WEATHER_SUNNY;
    if (strstr(desc,"\xe4\xba\x91")) return WEATHER_PARTLY_CLOUDY;
    return WEATHER_SUNNY;
}
