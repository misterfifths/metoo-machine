// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "app_wifi.h"
#include "app_sntp.h"

#include "twitter_task.h"
#include "audio_task.h"
#include "battery_task.h"


static const char *TAG = "APP";


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

	xTaskCreate(&audio_task_main, "audio_task", audio_task_stack_size, NULL, 4, NULL);

	init_networking();

	// You're supposed to wait until wifi starts before attempting to read from ADC1, which
	// is what the battery voltage is on.
	xTaskCreate(&battery_task_main, "battery_task", battery_task_stack_size, NULL, 3, NULL);

	xTaskCreate(&twitter_task_main, "twitter_task", twitter_task_stack_size, NULL, 5, NULL);
}
