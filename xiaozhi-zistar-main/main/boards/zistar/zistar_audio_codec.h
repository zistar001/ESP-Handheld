#ifndef _ZISTAR_AUDIO_CODEC_H
#define _ZISTAR_AUDIO_CODEC_H

#include "audio_codec.h"
#include <driver/gpio.h>
#include <mutex>

class ZistarAudioCodec : public AudioCodec {
private:
    std::mutex data_if_mutex_;
    gpio_num_t spkr_en_pin_;

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;

public:
    ZistarAudioCodec(int input_sample_rate, int output_sample_rate,
        gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
        gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data,
        gpio_num_t spkr_enable = GPIO_NUM_NC);
    virtual ~ZistarAudioCodec();
    virtual void SetOutputVolume(int volume) override;
};

#endif
