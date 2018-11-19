// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "app_wifi.h"
#include "sleep_task.h"
#include "audio_task.h"

#include "main.h"


const uint32_t sleep_task_stack_size = 2 * 1024;

static const char *TAG = "SLEEP";

// Must be an RTC-capable GPIO: 0, 2, 4, 12-15, 25-27, 32-39.
// Additionally, GPIO 34-39 do not have internal pullup/pulldown.
static const gpio_num_t sleep_wake_pin = GPIO_NUM_4;
static const int sleep_wake_level = 0;  // 0 = low, 1 = high

static xQueueHandle sleep_event_semaphore;


__attribute__((noreturn))
static void enter_deep_sleep(void)
{
	suspend_tasks_for_sleep();
	app_wifi_stop();

	esp_sleep_enable_ext0_wakeup(sleep_wake_pin, sleep_wake_level);

	// We need RTC peripherals for the pin wakeup, but we can disable everything else.
	// (The docs say this should happen automatically, but log messages indicate otherwise?)
	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
	esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

	esp_deep_sleep_start();
}


static void gpio_isr_handler(void *arg)
{
	// TODO: make this only trigger after a certain amount of time/triggers
	xSemaphoreGiveFromISR(sleep_event_semaphore, NULL);
}


void sleep_task_main(void *task_params)
{
	// If we just woke from sleep, our GPIO pin is configured for the RTC subsystem,
	// and we need to undo that.
	rtc_gpio_deinit(sleep_wake_pin);

	sleep_event_semaphore = xSemaphoreCreateBinary();


	gpio_config_t pin_config = {
		.mode = GPIO_MODE_INPUT,
		.intr_type = GPIO_PIN_INTR_NEGEDGE,
		.pin_bit_mask = 1ULL << sleep_wake_pin,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE
	};

	ESP_ERROR_CHECK(gpio_config(&pin_config));

	ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
	ESP_ERROR_CHECK(gpio_isr_handler_add(sleep_wake_pin, gpio_isr_handler, NULL));


    while(1) {
    	if(xSemaphoreTake(sleep_event_semaphore, portMAX_DELAY) != pdTRUE) continue;

    	ESP_LOGI(TAG, "Going into deep sleep!");

    	audio_task_empty_queue();
    	audio_task_enqueue_sound(audio_task_sound_error);
    	vTaskDelay(600 / portTICK_PERIOD_MS);

    	vTaskDelay(1000 / portTICK_PERIOD_MS);

    	enter_deep_sleep();
    }
}
