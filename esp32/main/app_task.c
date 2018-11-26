// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "esp_log.h"

#include "app_task.h"


TaskHandle_t app_task_create(const app_task_descriptor *descriptor)
{
	TaskHandle_t handle = NULL;
	BaseType_t res = xTaskCreate(descriptor->task_main,
								 descriptor->name,
								 descriptor->stack_size,
								 NULL,
								 descriptor->priority,
								 &handle);

	if(res != pdPASS) {
		ESP_LOGE("APP", "Error creating task '%s': %d", descriptor->name, res);
		abort();
	}

	return handle;
}
