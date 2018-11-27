// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_adc_cal.h"

#include "driver/adc.h"

#include "battery_task.h"
#include "audio_task.h"


static const char *TAG = "BATT";


void battery_task_main(void *task_params);
const app_task_descriptor battery_task_descriptor = {
	.task_main = battery_task_main,
	.name = "battery_task",
	.stack_size = 2 * 1024,
	.priority = 3
};


static const uint32_t warning_voltage_threshold_mV = 3300;  // See note below


void battery_task_main(void *task_params)
{
	// RTC_MODULE has a lot to say about the lock for ADC1
	esp_log_level_set("RTC_MODULE", ESP_LOG_INFO);


    adc1_config_width(ADC_WIDTH_12Bit);

    // ADC1 channel 7 is GPIO #35, which on the HUZZAH32/ESP32 Feather is half the voltage of the battery.
    // See https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts#gpio-and-analog-pins and
    // https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/power-management#measuring-battery
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

    esp_adc_cal_characteristics_t characteristics;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &characteristics);

    while(1) {
        uint32_t voltage;
        esp_adc_cal_get_voltage(ADC1_CHANNEL_7, &characteristics, &voltage);
        voltage *= 2;

        // Quoth Adafruit:
        // "Lipoly batteries are 'maxed out' at 4.2V and stick around 3.7V for much of the battery life,
        // then slowly sink down to 3.2V or so before the protection circuitry cuts it off."
        if(voltage < warning_voltage_threshold_mV) {
        	ESP_LOGW(TAG, "Low battery! %d mV", voltage);
        	audio_task_enqueue_sound(audio_task_sound_low_battery);
        }

        vTaskDelay(60 * 1000 / portTICK_RATE_MS);
    }
}
