// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "audio_task.h"
#include "audio_output.h"
#include "sound_data.h"


const uint32_t audio_task_stack_size = 2 * 1024;

static const char *TAG = "AUDIO";

#define CONFIG_AUDIO_TASK_QUEUE_LENGTH 64
static QueueHandle_t sound_queue = NULL;

static bool next_tweet_sound_is_tick = true;


void audio_task_main(void *task_params)
{
	audio_init();

	sound_queue = xQueueCreate(CONFIG_AUDIO_TASK_QUEUE_LENGTH, sizeof(audio_task_sound));
	audio_task_enqueue_sound(audio_task_sound_success1);

	while(1) {
		audio_task_sound sound_to_play;
		xQueueReceive(sound_queue, &sound_to_play, portMAX_DELAY);

		const unsigned char *sound_samples;
		unsigned int sound_samples_len;

		switch(sound_to_play) {
			case audio_task_sound_success1:
				sound_samples = sound_success1_samples;
				sound_samples_len = sound_success1_samples_len;
				break;

			case audio_task_sound_success2:
				sound_samples = sound_success2_samples;
				sound_samples_len = sound_success2_samples_len;
				break;

			case audio_task_sound_success3:
				sound_samples = sound_success3_samples;
				sound_samples_len = sound_success3_samples_len;
				break;

			case audio_task_sound_error:
				sound_samples = sound_error_samples;
				sound_samples_len = sound_error_samples_len;
				break;

			case audio_task_sound_tweet: {
				if(next_tweet_sound_is_tick) {
					sound_samples = sound_tick_samples;
					sound_samples_len = sound_tick_samples_len;
				}
				else {
					sound_samples = sound_tock_samples;
					sound_samples_len = sound_tock_samples_len;
				}

				next_tweet_sound_is_tick = !next_tweet_sound_is_tick;

				break;
			}

			case audio_task_sound_low_battery:
				sound_samples = sound_low_battery_samples;
				sound_samples_len = sound_low_battery_samples_len;
				break;

			default:
				ESP_LOGW(TAG, "Unknown sound ID %d; ignoring", sound_to_play);
				continue;
		}

		play_sound(sound_samples, sound_samples_len, true);
	}
}


bool audio_task_enqueue_sound(audio_task_sound sound)
{
	if(sound_queue) {
		return xQueueSend(sound_queue, &sound, 0) == pdTRUE;
	}

	ESP_LOGW(TAG, "audio_task_enqueue_sound called before the queue was initialized");
	return false;
}


void audio_task_empty_queue(void)
{
	if(sound_queue) xQueueReset(sound_queue);
}
