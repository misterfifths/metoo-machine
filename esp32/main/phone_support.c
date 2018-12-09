// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "phone_support.h"

#if CONFIG_TARGET_PHONE


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"

#include "audio_task.h"


static const char *TAG = "PHONE";


void phone_task_main(void *task_params);
const app_task_descriptor phone_task_descriptor = {
	.task_main = phone_task_main,
	.name = "phone_task",
	.stack_size = 2 * 1024,
	.priority = 4
};

void phone_handset_led_task_main(void *task_params);
const app_task_descriptor phone_handset_led_task_descriptor = {
	.task_main = phone_handset_led_task_main,
	.name = "handset_led_task",
	.stack_size = configMINIMAL_STACK_SIZE,
	.priority = 2
};


static const TickType_t switch_debounce_ticks = pdMS_TO_TICKS(100);

static const gpio_num_t amp_sdl_pin = GPIO_NUM_32;
static const gpio_num_t amp_sdr_pin = GPIO_NUM_14;
static const gpio_num_t handset_led_pin = GPIO_NUM_21;
static const gpio_num_t handset_switch_pin = GPIO_NUM_34;
static const gpio_num_t status_led_pin = GPIO_NUM_27;

static bool last_handset_switch_value = true;  // TODO: assuming we start with the phone on the hook

static TaskHandle_t phone_task_handle = NULL;
static TaskHandle_t phone_handset_led_task_handle = NULL;

static audio_task_sound last_handset_audio = audio_task_sound_handset_4;


static void init_amp_select_pins()
{
	gpio_config_t pin_config = {
		.mode = GPIO_MODE_OUTPUT,
		.intr_type = GPIO_PIN_INTR_DISABLE,
		.pin_bit_mask = (1ULL << amp_sdl_pin) | (1ULL << amp_sdr_pin),
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE
	};

	ESP_ERROR_CHECK(gpio_config(&pin_config));
}


static void init_led_pins()
{
	gpio_config_t pin_config = {
		.mode = GPIO_MODE_OUTPUT,
		.intr_type = GPIO_PIN_INTR_DISABLE,
		.pin_bit_mask = (1ULL << handset_led_pin) | (1ULL << status_led_pin),
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE
	};

	ESP_ERROR_CHECK(gpio_config(&pin_config));
}


static void handset_switch_isr_handler(void *arg)
{
	BaseType_t need_context_switch = xTaskResumeFromISR(phone_task_handle);
	if(need_context_switch == pdTRUE) {
		// If the task resume returned true, we need a context switch so we return from the
		// ISR into the resumed task.
		portYIELD_FROM_ISR();
	}
}


static void init_handset_switch_isr()
{
	gpio_config_t switch_pin_config = {
		.mode = GPIO_MODE_INPUT,
		.intr_type = GPIO_PIN_INTR_ANYEDGE,
		.pin_bit_mask = 1ULL << handset_switch_pin,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE
	};

	ESP_ERROR_CHECK(gpio_config(&switch_pin_config));

	// TODO: this call will fail if the service is already installed.
	// Not currently the case, but something to watch out for.
	ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
	ESP_ERROR_CHECK(gpio_isr_handler_add(handset_switch_pin, handset_switch_isr_handler, NULL));
}


void phone_handset_led_set(bool on)
{
	ESP_ERROR_CHECK(gpio_set_level(handset_led_pin, on ? 1 : 0));
}


void phone_status_led_set(bool on)
{
	ESP_ERROR_CHECK(gpio_set_level(status_led_pin, on ? 1 : 0));
}

void phone_status_led_flash(uint8_t flash_count)
{
	const TickType_t interval_ms = 100;

	for(int flash = 0; flash < flash_count; flash++) {
		phone_status_led_set(true);
		vTaskDelay(interval_ms / portTICK_PERIOD_MS);
		phone_status_led_set(false);

		if(flash != flash_count - 1) {
			vTaskDelay(interval_ms / portTICK_PERIOD_MS);
		}
	}
}


// This is rather low-level; probably use phone_set_audio_target.
static void phone_enable_amp_channels(bool enable_left, bool enable_right)
{
	ESP_ERROR_CHECK(gpio_set_level(amp_sdl_pin, enable_left ? 1 : 0));
	ESP_ERROR_CHECK(gpio_set_level(amp_sdr_pin, enable_right ? 1 : 0));
}


void phone_set_audio_target(phone_audio_target target)
{
	// Speaker = left, handset = right
	bool enable_left = (target & phone_audio_target_speaker) == phone_audio_target_speaker;
	bool enable_right = (target & phone_audio_target_handset) == phone_audio_target_handset;

	phone_enable_amp_channels(enable_left, enable_right);
}


bool phone_is_handset_on_hook()
{
	// TODO: this could theoretically disagree with the internal state, since
	// we're debouncing in the ISR.
	// Return last_handset_switch_value here instead of the direct GPIO read?

	return gpio_get_level(handset_switch_pin) == 0;
}


static void phone_init()
{
	init_led_pins();
	phone_status_led_set(false);
	phone_handset_led_set(false);

	init_amp_select_pins();
	phone_set_audio_target(phone_audio_target_speaker);  // TODO: another place where we assume the phone starts on the hook

	init_handset_switch_isr();
}


void phone_handset_led_blink()
{
	// Clear any existing task notification (which is used to interrupt a blink)
	xTaskNotify(phone_handset_led_task_handle, 0, eSetValueWithOverwrite);

	// Wake the task
	vTaskResume(phone_handset_led_task_handle);
}


void phone_handset_led_blink_from_ISR()
{
	// Clear any existing task notification (which is used to interrupt a blink).
	// We can ignore the last argument since we're clearing the value, not setting it.
	// The next call will probably wake the task anyway.
	xTaskNotifyFromISR(phone_handset_led_task_handle, 0, eSetValueWithOverwrite, NULL);

	BaseType_t need_context_switch = xTaskResumeFromISR(phone_handset_led_task_handle);
	if(need_context_switch == pdTRUE) {
		// If the task resume returned true, we need a context switch so we return from the
		// ISR into the resumed task.
		portYIELD_FROM_ISR();
	}
}


void phone_handset_led_blink_stop()
{
	xTaskNotifyGive(phone_handset_led_task_handle);
}


void phone_handset_led_blink_stop_from_ISR()
{
	BaseType_t need_context_switch = pdFALSE;
	vTaskNotifyGiveFromISR(phone_handset_led_task_handle, &need_context_switch);
	if(need_context_switch == pdTRUE) {
		// If the task resume returned true, we need a context switch so we return from the
		// ISR into the resumed task.
		portYIELD_FROM_ISR();
	}
}


void phone_handset_led_task_main(void *task_params)
{
	phone_handset_led_task_handle = xTaskGetCurrentTaskHandle();

	// From looking at the ring waveform...
	const TickType_t intro_pause_ms = 270;
	const TickType_t outro_pause_ms = 190;
	const uint8_t pulse_cycles = 2;
	const uint8_t on_pulses_per_cycle = 6;
	const TickType_t on_pulse_duration_ms = 30;
	const TickType_t off_pulse_duration_ms = on_pulse_duration_ms;
	const TickType_t pause_between_cycles_ms = 200;


	#define delay_on_task_notification(ms) \
		do { \
			if(ulTaskNotifyTake(pdTRUE, ms / portTICK_PERIOD_MS) != 0) { \
				goto suspend; \
			} \
		} while(0)


	/* Here's the gameplan:
	 * This task immediately suspends itself.
	 * A call to phone_handset_led_blink wakes it, and it begins a tight loop of
	 * turning on & off the LED.
	 * Throughout that loop, it checks on its direct-to-task notification. If it
	 * ever becomes non-zero, it aborts the blinking loop, turns off the LED, and
	 * goes back to sleep.
	 * phone_handset_led_blink_stop just sets the task notification.
	 * phone_handset_led_blink makes sure to clear it before waking the task,
	 * in case there was an unbalanced call to phone_handset_led_blink_stop.
	 *
	 * Any logic about whether this thing should blink at all (i.e., based on
	 * whether the handset is on the hook) happens elsewhere.
	 */

	while(1) {
		suspend:
		phone_handset_led_set(false);
		vTaskSuspend(NULL);

		delay_on_task_notification(intro_pause_ms);

		for(uint8_t cycle = 0; cycle < pulse_cycles; cycle++) {
			for(uint8_t pulse = 0; pulse < on_pulses_per_cycle; pulse++) {
				phone_handset_led_set(true);
				delay_on_task_notification(on_pulse_duration_ms);

				phone_handset_led_set(false);
				delay_on_task_notification(off_pulse_duration_ms);
			}

			delay_on_task_notification(pause_between_cycles_ms);
		}

		delay_on_task_notification(outro_pause_ms);
	}
}


static void handle_phone_picked_up()
{
	// Kill any blinking of the handset LED.
	// Mute & kill any pending audio.
	// Play the next testimonial.

	phone_handset_led_blink_stop();
	phone_set_audio_target(phone_audio_target_mute);
	audio_task_empty_queue();

	switch(last_handset_audio) {
		case audio_task_sound_handset_1:
			last_handset_audio = audio_task_sound_handset_2;
		break;

		case audio_task_sound_handset_2:
			last_handset_audio = audio_task_sound_handset_3;
		break;

		case audio_task_sound_handset_3:
			last_handset_audio = audio_task_sound_handset_4;
		break;

		case audio_task_sound_handset_4:
		default:
			last_handset_audio = audio_task_sound_handset_1;
		break;
	}

	audio_task_enqueue_sound(last_handset_audio);
}


static void handle_phone_hung_up()
{
	// Mute & kill any pending audio.
	phone_set_audio_target(phone_audio_target_mute);
	audio_task_empty_queue();
}


void phone_task_main(void *task_params)
{
	phone_task_handle = xTaskGetCurrentTaskHandle();
	phone_init();


	TickType_t ticks_at_last_wake = xTaskGetTickCount();

	while(1) {
		// Go to sleep. The ISR will wake us if the phone goes off the hook
		vTaskSuspend(NULL);

		const TickType_t current_ticks = xTaskGetTickCount();
		const TickType_t elapsed_ticks = current_ticks - ticks_at_last_wake;
		ticks_at_last_wake = current_ticks;
		if(elapsed_ticks < switch_debounce_ticks) {
			// This event came too close to another. Sleep for a bit before we
			// check the switch's status.
			vTaskDelayUntil(&ticks_at_last_wake, switch_debounce_ticks);  // TODO: sleep for switch_debounce_ticks - elapsed_ticks instead?
		}

		bool on_hook = phone_is_handset_on_hook();

		if(on_hook != last_handset_switch_value) {
			ESP_LOGI(TAG, "Handset switch change - %son hook", on_hook ? "" : "not ");
			last_handset_switch_value = on_hook;

			if(on_hook) {
				handle_phone_hung_up();
			}
			else {
				handle_phone_picked_up();
			}
		}
	}
}


#endif
