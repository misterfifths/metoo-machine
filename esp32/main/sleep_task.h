// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "sdkconfig.h"

#if !CONFIG_TARGET_PHONE && !defined(_SLEEP_TASK_H)
#define _SLEEP_TASK_H


#include "app_task.h"


extern const app_task_descriptor sleep_task_descriptor;


#endif
