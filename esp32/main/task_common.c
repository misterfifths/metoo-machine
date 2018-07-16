// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "task_common.h"
#include "esp_log.h"


SemaphoreHandle_t tweet_semaphore;

static const char *TAG = "CMN";


void task_common_init()
{
	tweet_semaphore = xSemaphoreCreateCounting(100, 0);
	if(tweet_semaphore == NULL) {
		ESP_LOGE(TAG, "Couldn't create semaphore!");
	}
}
