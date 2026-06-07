#include "es8311_driver.h"
#include "box_audio_codec.h"

esp_err_t es8311_init(void)
{
    return box_audio_init();
}

void es8311_set_volume(uint8_t vol)
{
    box_audio_set_volume(vol);
}

uint8_t es8311_get_volume(void)
{
    return box_audio_get_volume();
}

esp_err_t es8311_write(const int16_t *data, int samples)
{
    return box_audio_write(data, samples);
}

void es8311_stop(void)
{
    box_audio_stop();
}

bool es8311_is_initialized(void)
{
    return box_audio_is_inited();
}
