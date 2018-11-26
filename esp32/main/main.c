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


static TaskHandle_t create_task(TaskFunction_t task_main, const char * const name, uint32_t stack_size, UBaseType_t priority)
{
	TaskHandle_t handle = NULL;
	BaseType_t res = xTaskCreate(task_main, name, stack_size, NULL, priority, &handle);

	if(res != pdPASS) {
		ESP_LOGE(TAG, "Error creating task '%s': %d", name, res);
		abort();
	}

	return handle;
}


void app_main()
{
	ESP_LOGI(TAG, "Hello, world!");
	if(esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
		ESP_LOGI(TAG, "This is a wake from deep sleep.");
	}

	create_task(audio_task_main, "audio_task", audio_task_stack_size, 4);

	create_task(sleep_task_main, "sleep_task", sleep_task_stack_size, 10);

	init_networking();

	// You're supposed to wait until wifi starts before attempting to read from ADC1, which
	// is what the battery voltage is on.
	create_task(battery_task_main, "battery_task", battery_task_stack_size, 3);

	twitter_task_handle = create_task(twitter_task_main, "twitter_task", twitter_task_stack_size, 5);
}
