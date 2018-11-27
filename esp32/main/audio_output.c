// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/i2s.h"

#include "audio_output.h"
#include "sound_data.h"


static const char *TAG = "AUDIO_OUT";

#define CONFIG_I2S_NUM I2S_NUM_0


// Note that these values (sample rate, bits/sample, and mono/stereo) must match the incoming
// sample buffers to play_sound (and thus the settings in the make_audio_header script) exactly;
// no conversion is attempted.

#define CONFIG_I2S_SAMPLE_RATE 16000

// Bits/sample must be either 16 or 32... using I2S_BITS_PER_SAMPLE_8BIT always gave me an error.
// Note that using the DAC, the 8 MSBs of each sample is all that will be used, regardless of this setting
#define CONFIG_I2S_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT

// It's unclear to me what this means, exactly -- I want mono output (and have mono input),
// so I thought I2S_CHANNEL_FMT_ONLY_RIGHT would be what I want, but that never seems to work.
#define CONFIG_I2S_CHANNEL_FORMAT I2S_CHANNEL_FMT_RIGHT_LEFT

// Again unclear on this; it seems to me that I2S_CHANNEL_MONO with a 2-channel output would be
// some sort of configuration error, but this is what works...
#define CONFIG_I2S_CHANNEL_COUNT I2S_CHANNEL_MONO


// See note in audio_init below about these weirdnesses.
#define CONFIG_I2S_DMA_BUF_COUNT 2  // 2 is the minimum
#define CONFIG_I2S_DMA_BUF_LEN 64

#define _CHANNEL_COUNT (CONFIG_I2S_CHANNEL_FORMAT < I2S_CHANNEL_FMT_ONLY_RIGHT ? 2 : 1)
#define silence_samples_len (CONFIG_I2S_DMA_BUF_COUNT * CONFIG_I2S_DMA_BUF_LEN * (CONFIG_I2S_BITS_PER_SAMPLE / 8) * _CHANNEL_COUNT)
static unsigned char silence_samples[silence_samples_len] = { 0 };


void audio_init()
{
	// the i2s module is chatty about DMA buffers
	esp_log_level_set("I2S", ESP_LOG_INFO);

	esp_log_level_set(TAG, ESP_LOG_INFO);


    i2s_config_t i2s_config = {
		.mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
		.sample_rate = CONFIG_I2S_SAMPLE_RATE,
		.bits_per_sample = CONFIG_I2S_BITS_PER_SAMPLE,
		.channel_format = CONFIG_I2S_CHANNEL_FORMAT,
		.communication_format = I2S_COMM_FORMAT_I2S_MSB,  // despite the audio being technically PCM, this is the format you want, apparently
		.intr_alloc_flags = 0,
		.dma_buf_count = CONFIG_I2S_DMA_BUF_COUNT,
		.dma_buf_len = CONFIG_I2S_DMA_BUF_LEN
    };


    ESP_ERROR_CHECK(i2s_driver_install(CONFIG_I2S_NUM, &i2s_config, 0, NULL));

    // Yet another source of confusion for me re: mono/stereo.
    // I2S_DAC_CHANNEL_BOTH_EN is the only thing that I got working.
    ESP_ERROR_CHECK(i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN));

    // Why do you need to specify sample rate and bits/sample in i2s_driver_install and also with this?
    ESP_ERROR_CHECK(i2s_set_clk(CONFIG_I2S_NUM, CONFIG_I2S_SAMPLE_RATE, CONFIG_I2S_BITS_PER_SAMPLE, CONFIG_I2S_CHANNEL_COUNT));


    /*
	* Re: i2s_config_t.dma_buf_len and noise: here's what I've gleaned.
	* If your audio samples sent via i2s_write don't have a certain amount of silence at the end, then
	* you'll get a nasty droning sound after your sound finishes.
	* This seems to be an underrun of the DMA buffers (see, e.g., https://github.com/earlephilhower/ESP8266Audio/issues/48).
	* Calling i2s_zero_dma_buffer before i2s_write seems like it would be the ideal solution,
	* but it causes popping sounds (probably because "silence" isn't just zero bytes).
	*
	* If we work on the suspicion that we need to fill the DMA buffers with (actual) silence after a
	* sound, we can find the formula for the size of the DMA buffers in i2s.c, function i2s_create_dma_queue():
	* 	dma_buf_count * dma_buf_len * bytes_per_sample * channel_count
	*
	* So to work around, here we calculate that size and make a buffer full of that many bytes of silence.
	* The script that generates sound_data.{c,h} also generates one sample of silence in the same format as
	* the rest of the audio. We expand that to a longer buffer below, and play_sound() writes that buffer
	* to i2s after every sound.
	*/

    ESP_LOGI(TAG, "Will be appending %zu bytes of silence to each sound", silence_samples_len);

    size_t template_i = 0;
    for(size_t i = 0; i < silence_samples_len; i++) {
    	silence_samples[i] = sound_silence_sample[template_i];

    	template_i = (template_i + 1) % sound_silence_sample_len;
    }
}


void play_sound(const unsigned char *samples, size_t samples_length, bool sync)
{
	const int mono_divisor = CONFIG_I2S_CHANNEL_COUNT == I2S_CHANNEL_MONO ? 2 : 1;
	const size_t total_samples_length = samples_length + silence_samples_len;
	const uint32_t sound_length_ms = ((double)total_samples_length / CONFIG_I2S_SAMPLE_RATE / mono_divisor) * 1000;
	ESP_LOGD(TAG, "Playing %zu samples, for a length of %u ms", total_samples_length, sound_length_ms);

	size_t bytes_written;
	ESP_ERROR_CHECK(i2s_write(CONFIG_I2S_NUM, samples, samples_length, &bytes_written, portMAX_DELAY));

	// See big fat note in audio_init() about the purpose and duration of this silence.
	ESP_ERROR_CHECK(i2s_write(CONFIG_I2S_NUM, silence_samples, silence_samples_len, &bytes_written, portMAX_DELAY));

	if(sync) {
		vTaskDelay(sound_length_ms / portTICK_PERIOD_MS);
		ESP_LOGD(TAG, "Audio done");
	}
}
