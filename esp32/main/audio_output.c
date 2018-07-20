// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "audio_output.h"


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
		.dma_buf_count = 2, // 2 is the minimum
		.dma_buf_len = 64,  // seems this needs to be significantly smaller than the audio sample you're playing, or things get stuttery
    };


    ESP_ERROR_CHECK(i2s_driver_install(CONFIG_I2S_NUM, &i2s_config, 0, NULL));

    // Yet another source of confusion for me re: mono/stereo.
    // I2S_DAC_CHANNEL_BOTH_EN is the only thing that I got working.
    ESP_ERROR_CHECK(i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN));

    // Why do you need to specify sample rate and bits/sample in i2s_driver_install and also with this?
    ESP_ERROR_CHECK(i2s_set_clk(CONFIG_I2S_NUM, CONFIG_I2S_SAMPLE_RATE, CONFIG_I2S_BITS_PER_SAMPLE, CONFIG_I2S_CHANNEL_COUNT));
}


void play_sound(const unsigned char *samples, size_t samples_length, bool sync)
{
	const int mono_divisor = CONFIG_I2S_CHANNEL_COUNT == I2S_CHANNEL_MONO ? 2 : 1;
	const uint32_t sound_length_ms = ((double)samples_length / CONFIG_I2S_SAMPLE_RATE / mono_divisor) * 1000;
	ESP_LOGD(TAG, "Playing %zu samples, for a length of %u ms", samples_length, sound_length_ms);

	size_t bytes_written;
	ESP_ERROR_CHECK(i2s_write(CONFIG_I2S_NUM, samples, samples_length, &bytes_written, portMAX_DELAY));
//	ESP_LOGD(TAG, "Wrote %zu bytes of audio (%zu total)", bytes_written, samples_length);

	if(sync) {
		vTaskDelay(sound_length_ms / portTICK_PERIOD_MS);
		ESP_LOGD(TAG, "Audio done");
	}
}
