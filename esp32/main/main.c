// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "app_wifi.h"
#include "app_sntp.h"

#include "task_common.h"
#include "twitter_task.h"
#include "audio_task.h"


static const char *TAG = "APP";


static void early_init(void)
{
	task_common_init();

	// The ESP HTTP client log level is debug by default, and it is *chatty*
	esp_log_level_set("HTTP_CLIENT", ESP_LOG_INFO);
}


static void init_networking(void)
{
	app_wifi_initialize();
	app_wifi_wait_connected();

	app_sntp_initialize();
	app_sntp_wait_for_time_update(10);
}

void app_main()
{
	ESP_LOGI(TAG, "Hello, world!");

	early_init();
	init_networking();

	xTaskCreate(&twitter_task_main, "twitter_task", twitter_task_stack_size, NULL, 5, NULL);
	xTaskCreate(&audio_task_main, "audio_task", audio_task_stack_size, NULL, 4, NULL);
}
