#include "es7210_driver.h"
#include "box_audio_codec.h"

esp_err_t es7210_init(void)
{
    return box_audio_init();
}

esp_err_t es7210_read(int16_t *data, int samples)
{
    return box_audio_read(data, samples);
}

void es7210_set_gain(uint8_t db)
{
    box_audio_set_mic_gain(db);
}

void es7210_stop(void)
{
    box_audio_stop();
}

bool es7210_is_initialized(void)
{
    return box_audio_is_inited();
}
