// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"

#include "sdkconfig.h"

#include "audio_task.h"
#include "task_common.h"


#define BLINK_GPIO CONFIG_BLINK_GPIO

const uint32_t audio_task_stack_size = 2 * 1024;

static const char *TAG = "AUDIO";


void audio_task_main(void *task_params)
{
	ESP_LOGI(TAG, "Outputting on GPIO %d", BLINK_GPIO);

	gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_GPIO, 0);

    while(1) {
        if(xSemaphoreTake(tweet_semaphore, 200) != pdTRUE) continue;

        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, 0);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
