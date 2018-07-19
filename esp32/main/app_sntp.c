// 2018 / Tim Clem / github.com/misterfifths
// Adapted from Espressif ESP IDF sample code.
// Public domain.

#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "apps/sntp/sntp.h"
#include "app_sntp.h"


static const char *TAG = "SNTP";
static const uint16_t SNTPRetryIntervalMS = 200;  // millis to wait between SNTP retries


static bool time_is_set(void)
{
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	// Is time set? If not, tm_year will be (1970 - 1900).
	return timeinfo.tm_year > (2016 - 1900);
}


static void log_time(void)
{
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];

	time(&now);
	localtime_r(&now, &timeinfo);

	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "Current time: %s", strftime_buf);
}


void app_sntp_initialize(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}


bool app_sntp_wait_for_time_update(uint8_t max_retries)
{
	uint8_t remaining_tries = max_retries + 1;

	while(!time_is_set() && remaining_tries != 0) {
		--remaining_tries;

		ESP_LOGI(TAG, "Waiting for system time to be set (%u tries remaining)...", remaining_tries);
		vTaskDelay(SNTPRetryIntervalMS / portTICK_PERIOD_MS);
	}

	if(time_is_set()) {
		log_time();
		return true;
	}

	ESP_LOGW(TAG, "System time didn't update!");
	log_time();
	return false;
}
