// 2018 / Tim Clem / github.com/misterfifths
// Adapted from Espressif ESP IDF sample code.
// Public domain.

#ifndef _APP_SNTP_H_
#define _APP_SNTP_H_


#include <stdint.h>
#include <stdbool.h>


void app_sntp_initialize(void);
bool app_sntp_wait_for_time_update(uint8_t max_retries);


#endif
