// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "audio_task.h"
#include "audio_output.h"
#include "sound_data.h"
#include "handset_sound_data.h"
#include "phone_support.h"


static const char *TAG = "AUDIO";


void audio_task_main(void *task_params);
const app_task_descriptor audio_task_descriptor = {
	.task_main = audio_task_main,
	.name = "audio_task",
	.stack_size = 4 * 1024,
	.priority = 4
};


#define CONFIG_AUDIO_TASK_QUEUE_LENGTH 64
static QueueHandle_t sound_queue = NULL;

#if !CONFIG_TARGET_PHONE
static bool next_tweet_sound_is_tick = true;
#endif


void audio_task_main(void *task_params)
{
	audio_init();

	sound_queue = xQueueCreate(CONFIG_AUDIO_TASK_QUEUE_LENGTH, sizeof(audio_task_sound));
	audio_task_enqueue_sound(audio_task_sound_success1);

	while(1) {
		audio_task_sound sound_to_play;
		xQueueReceive(sound_queue, &sound_to_play, portMAX_DELAY);

		const unsigned char *sound_samples = NULL;
		unsigned int sound_samples_len = 0;

		#if CONFIG_TARGET_PHONE
		uint8_t status_led_flash_count = 0;
		phone_audio_target audio_target = phone_audio_target_mute;

		const TickType_t handset_audio_start_delay_ms = 700;
		TickType_t sound_start_delay_ms = 0;

		const TickType_t ring_recovery_delay_ms = 400;
		#endif

		switch(sound_to_play) {
			case audio_task_sound_success1:
				#if CONFIG_TARGET_PHONE
				status_led_flash_count = 1;
				#else
				sound_samples = sound_success1_samples;
				sound_samples_len = sound_success1_samples_len;
				#endif
				break;

			case audio_task_sound_success2:
				#if CONFIG_TARGET_PHONE
				status_led_flash_count = 2;
				#else
				sound_samples = sound_success2_samples;
				sound_samples_len = sound_success2_samples_len;
				#endif
				break;

			case audio_task_sound_success3:
				#if CONFIG_TARGET_PHONE
				status_led_flash_count = 3;
				#else
				sound_samples = sound_success3_samples;
				sound_samples_len = sound_success3_samples_len;
				#endif
				break;

			case audio_task_sound_error:
				#if CONFIG_TARGET_PHONE
				phone_status_led_set(true);
				#else
				sound_samples = sound_error_samples;
				sound_samples_len = sound_error_samples_len;
				#endif
				break;

			case audio_task_sound_tweet: {
				#if CONFIG_TARGET_PHONE
				audio_target = phone_audio_target_speaker;
				sound_samples = ring_samples;
				sound_samples_len = ring_samples_len;
				#else
				if(next_tweet_sound_is_tick) {
					sound_samples = sound_tick_samples;
					sound_samples_len = sound_tick_samples_len;
				}
				else {
					sound_samples = sound_tock_samples;
					sound_samples_len = sound_tock_samples_len;
				}

				next_tweet_sound_is_tick = !next_tweet_sound_is_tick;
				#endif

				break;
			}

			#if !CONFIG_TARGET_PHONE
			case audio_task_sound_low_battery:
				sound_samples = sound_low_battery_samples;
				sound_samples_len = sound_low_battery_samples_len;
				break;
			#endif

			#if CONFIG_TARGET_PHONE

			case audio_task_sound_handset_1:
				audio_target = phone_audio_target_handset;
				sound_start_delay_ms = handset_audio_start_delay_ms;

				sound_samples = handset_audio_1_samples;
				sound_samples_len = handset_audio_1_samples_len;
			break;

			case audio_task_sound_handset_2:
				audio_target = phone_audio_target_handset;
				sound_start_delay_ms = handset_audio_start_delay_ms;

				sound_samples = handset_audio_2_samples;
				sound_samples_len = handset_audio_2_samples_len;
			break;

			case audio_task_sound_handset_3:
				audio_target = phone_audio_target_handset;
				sound_start_delay_ms = handset_audio_start_delay_ms;

				sound_samples = handset_audio_3_samples;
				sound_samples_len = handset_audio_3_samples_len;
			break;

			case audio_task_sound_handset_4:
				audio_target = phone_audio_target_handset;
				sound_start_delay_ms = handset_audio_start_delay_ms;

				sound_samples = handset_audio_4_samples;
				sound_samples_len = handset_audio_4_samples_len;
			break;

			#endif

			default:
				ESP_LOGW(TAG, "Unknown sound ID %d; ignoring", sound_to_play);
				continue;
		}

		#if CONFIG_TARGET_PHONE
		bool phone_on_hook = phone_is_handset_on_hook();

		// Make sure we don't play handset audio when the phone's on the hook,
		// or speaker audio when it's off it.
		if((phone_on_hook && audio_target == phone_audio_target_handset) ||
		   (!phone_on_hook && audio_target == phone_audio_target_speaker))
		{
			sound_samples = NULL;
			sound_samples_len = 0;
		}

		if(sound_samples && sound_to_play == audio_task_sound_tweet) {
			// Blink the handset LED.
			// This is asynchronous, so will be roughly in time with the sound.
			phone_handset_led_blink();
		}

		if(status_led_flash_count != 0) {
			// This is synchronous, so delays the next "sound"
			phone_status_led_flash(status_led_flash_count);
		}

		if(sound_samples) {
			phone_set_audio_target(audio_target);
		}

		if(sound_start_delay_ms != 0) {
			vTaskDelay(sound_start_delay_ms / portTICK_PERIOD_MS);
		}
		#endif

		if(sound_samples) {
			play_sound(sound_samples, sound_samples_len, true);
		}

		#if CONFIG_TARGET_PHONE
		if(sound_samples && sound_to_play == audio_task_sound_tweet) {
			vTaskDelay(ring_recovery_delay_ms / portTICK_PERIOD_MS);
		}
		#endif
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
