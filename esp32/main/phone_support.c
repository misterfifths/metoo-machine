// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "phone_support.h"

#if CONFIG_TARGET_PHONE


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"


static const char *TAG = "PHONE";


void phone_task_main(void *task_params);
const app_task_descriptor phone_task_descriptor = {
	.task_main = phone_task_main,
	.name = "phone_task",
	.stack_size = 2 * 1024,
	.priority = 10
};


static const TickType_t switch_debounce_ticks = pdMS_TO_TICKS(100);

static const gpio_num_t amp_sdl_pin = GPIO_NUM_32;
static const gpio_num_t amp_sdr_pin = GPIO_NUM_14;
static const gpio_num_t handset_led_pin = GPIO_NUM_21;
static const gpio_num_t handset_switch_pin = GPIO_NUM_34;

static bool last_handset_switch_value = true;  // TODO: assuming we start with the phone on the hook

static TaskHandle_t phone_task_handle = NULL;


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


static void init_led_pin()
{
	gpio_config_t led_pin_config = {
		.mode = GPIO_MODE_OUTPUT,
		.intr_type = GPIO_PIN_INTR_DISABLE,
		.pin_bit_mask = 1ULL << handset_led_pin,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE
	};

	ESP_ERROR_CHECK(gpio_config(&led_pin_config));
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

	ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
	ESP_ERROR_CHECK(gpio_isr_handler_add(handset_switch_pin, handset_switch_isr_handler, NULL));
}


void phone_set_handset_led(bool on)
{
	ESP_ERROR_CHECK(gpio_set_level(handset_led_pin, on ? 1 : 0));
}


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
	// Switch is closed (high) when held down by the handset
	return gpio_get_level(handset_switch_pin) == 1;
}


static void phone_init()
{
	init_led_pin();
	phone_set_handset_led(false);

	init_amp_select_pins();
	phone_enable_amp_channels(true, false);  // TODO: another place where we assume the phone starts on the hook

	init_handset_switch_isr();
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

			// handset on switch = led on = audio over speaker
			// handset off switch = led off = audio over handset
			phone_set_handset_led(on_hook);
			phone_set_audio_target(on_hook ? phone_audio_target_speaker : phone_audio_target_handset);
		}
	}
}


#endif
