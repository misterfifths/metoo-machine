// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_sleep.h"

#include "main.h"

#include "app_wifi.h"
#include "app_sntp.h"

#include "app_task.h"
#include "twitter_task.h"
#include "audio_task.h"
#include "battery_task.h"
#include "sleep_task.h"
#include "phone_support.h"


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

	app_task_create(&audio_task_descriptor);

	#if CONFIG_TARGET_PHONE
	app_task_create(&phone_task_descriptor);
	#else
	app_task_create(&sleep_task_descriptor);
	#endif

	init_networking();

	#if !CONFIG_TARGET_PHONE
	// You're supposed to wait until wifi starts before attempting to read from ADC1, which
	// is what the battery voltage is on.
	app_task_create(&battery_task_descriptor);
	#endif

	twitter_task_handle = app_task_create(&twitter_task_descriptor);
}
