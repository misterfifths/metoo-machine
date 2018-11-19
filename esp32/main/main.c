// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_sleep.h"

#include "main.h"

#include "app_wifi.h"
#include "app_sntp.h"

#include "twitter_task.h"
#include "audio_task.h"
#include "battery_task.h"
#include "sleep_task.h"


static const char *TAG = "APP";

static TaskHandle_t twitter_task_handle;


void suspend_tasks_for_sleep(void)
{
	vTaskSuspend(twitter_task_handle);
}


static void init_networking(void)
{
	app_wifi_initialize();
	app_wifi_wait_connected();

	audio_task_enqueue_sound(audio_task_sound_success2);

	app_sntp_initialize();
	app_sntp_wait_for_time_update(10);
}


void app_main()
{
	ESP_LOGI(TAG, "Hello, world!");
	if(esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
		ESP_LOGI(TAG, "This is a wake from deep sleep.");
	}

	xTaskCreate(audio_task_main, "audio_task", audio_task_stack_size, NULL, 4, NULL);

	xTaskCreate(sleep_task_main, "sleep_task", sleep_task_stack_size, NULL, 10, NULL);

	init_networking();

	// You're supposed to wait until wifi starts before attempting to read from ADC1, which
	// is what the battery voltage is on.
	xTaskCreate(battery_task_main, "battery_task", battery_task_stack_size, NULL, 3, NULL);

	xTaskCreate(twitter_task_main, "twitter_task", twitter_task_stack_size, NULL, 5, &twitter_task_handle);
}
