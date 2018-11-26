// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef APP_TASK_H
#define APP_TASK_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


typedef struct {
	TaskFunction_t task_main;
	const char * const name;
	uint32_t stack_size;
	UBaseType_t priority;
} app_task_descriptor;


TaskHandle_t app_task_create(const app_task_descriptor *descriptor);


#endif
