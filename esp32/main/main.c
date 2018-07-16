// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "cJSON.h"

#include "sdkconfig.h"

#include "oauth.h"

#include "app_wifi.h"
#include "app_sntp.h"
#include "secrets.h"


#define BLINK_GPIO CONFIG_BLINK_GPIO


static const char *TAG = "APP";

static SemaphoreHandle_t blink_semaphore;


static void init_networking(void)
{
	app_wifi_initialize();
	app_wifi_wait_connected();

	app_sntp_initialize();
	app_sntp_wait_for_time_update(10);
}

static void pre_task_init(void)
{
	esp_log_level_set("HTTP_CLIENT", ESP_LOG_INFO);
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	blink_semaphore = xSemaphoreCreateCounting(100, 0);
	if(blink_semaphore == NULL) {
		ESP_LOGE(TAG, "Couldn't create blink semaphore!");
	}
}

static void read_loop(esp_http_client_handle_t http_client)
{
	const size_t resp_buffer_length = 20 * 1024;
	char *resp_buffer = calloc(resp_buffer_length, sizeof(char));

	ptrdiff_t next_read_offset = 0;
	size_t next_read_length = resp_buffer_length - 1;  // leave a null at the end of the buffer so cJSON doesn't go past it when parsing

	while(1) {
		ESP_LOGD(TAG, "Reading %zu bytes of the response into position %zd in the buffer (%zu + %zu = %zu)", next_read_length, next_read_offset, next_read_offset, next_read_length, next_read_length + next_read_offset);

		int bytes_read = esp_http_client_read(http_client, &resp_buffer[next_read_offset], next_read_length);
		if(bytes_read == -1) {
			ESP_LOGE(TAG, "Error reading response body");
			goto cleanup;
		}

		size_t valid_bytes_in_buffer = next_read_offset + bytes_read;
		ESP_LOGD(TAG, "Read %d bytes; there are %zu valid bytes in the buffer", bytes_read, valid_bytes_in_buffer);

		// It seems wise to do something like this to safeguard things in case we don't fill the buffer
		if(valid_bytes_in_buffer < resp_buffer_length) {
			resp_buffer[valid_bytes_in_buffer] = '\0';
		}


		const char *end_of_json_document = NULL;
		cJSON *json = cJSON_ParseWithOpts(resp_buffer, &end_of_json_document, 0);
		if(json == NULL) {
			// TODO: this might happen if we are unable to fit an entire tweet within the buffer
			// Hoping to mitigate that by just making the buffer sufficiently large.... risky business
			size_t error_offset = cJSON_GetErrorPtr() - resp_buffer;
			ESP_LOGE(TAG, "Unable to parse response as JSON (starting at character %zu): %s", error_offset, resp_buffer);
			goto cleanup;
		}


		// We get a variety of messages through this channel (disconnects, rate limits, user updates, etc.).
		// Tweets seem to be the only one with a "text" property, so I'm using that as the discriminator.
		// TODO: make note of disconnect messages, stall notifications, and *limit messages*!
		if(cJSON_HasObjectItem(json, "text")) {
			ESP_LOGI(TAG, "a tweet!");
			xSemaphoreGive(blink_semaphore);
		}
		else {
			char *jsonString = cJSON_Print(json);
			ESP_LOGI(TAG, "Not a tweet!\n%s", jsonString);
			free(jsonString);
		}

		cJSON_Delete(json);


		size_t length_of_json_document = end_of_json_document - resp_buffer;
		size_t length_of_next_json_fragment = valid_bytes_in_buffer - length_of_json_document;

		if(length_of_next_json_fragment > 0) {
			// shuffle the unread data to the front of the buffer
			memmove(resp_buffer, end_of_json_document + 1, length_of_next_json_fragment);

			next_read_offset = length_of_next_json_fragment - 1;  // this is an offset, so we need the - 1 to account for it begin 0-based
		}
		else {
			next_read_offset = 0;
		}

		next_read_length = resp_buffer_length - length_of_next_json_fragment;
		ESP_LOGD(TAG, "We read %d bytes. The JSON document was %zu bytes. That leaves %zu left in the buffer.", bytes_read, length_of_json_document, length_of_next_json_fragment);
	}

cleanup:
	free(resp_buffer);
}

static void do_stuff(void)
{
	const char *url = "https://stream.twitter.com/1.1/statuses/filter.json";

	int params_count = 0;
	char **params = NULL;
	oauth_add_param_to_array(&params_count, &params, url);
	oauth_add_param_to_array(&params_count, &params, "track=#metoo");

	char *postargs = NULL;
	// The return value of oauth_sign_array2 ("signed URL") is only useful for GETs, really.
	// We also don't need to explicitly free it, since it winds up in params. It'll get freed later
	// by oauth_free_array.
	oauth_sign_array2(&params_count, &params, &postargs, OA_HMAC, "POST", twitter_consumer_key, twitter_consumer_secret, twitter_access_token, twitter_access_token_secret);


	// incantation to generate the bulk of the Authorization header value
	char *auth_header_guts = oauth_serialize_url_sep(params_count, 1, params, ", ", 2 | 4);

	size_t auth_header_length = strlen(auth_header_guts) + 6 + 1;  // 6 = "OAuth "
	char *auth_header_value = malloc(auth_header_length);
	snprintf(auth_header_value, auth_header_length, "OAuth %s", auth_header_guts);
	free(auth_header_guts);

	// we're now done with our params array; everything we need is in postargs and auth_header_value
	oauth_free_array(&params_count, &params);


	esp_http_client_config_t http_config = {
		.url = url,
		.method = HTTP_METHOD_POST,
		.transport_type = HTTP_TRANSPORT_OVER_SSL
	};

	esp_http_client_handle_t http_client = esp_http_client_init(&http_config);

	esp_http_client_set_header(http_client, "Authorization", auth_header_value);
	free(auth_header_value);

	// esp_http_client_set_post_field sets fields on the client that are only used by
	// esp_http_client_perform, which we're not going to be using.
	// However, it also sets some useful headers (Content-Type, for instance), so we're
	// going to call it anyway.
	size_t postargs_length = strlen(postargs);
	esp_http_client_set_post_field(http_client, postargs, postargs_length);


	esp_err_t err;

	err = esp_http_client_open(http_client, postargs_length);
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "Error opening connection: %s", esp_err_to_name(err));
		goto cleanup;
	}


	int bytes_written = esp_http_client_write(http_client, postargs, postargs_length);
	if(bytes_written == -1) {
		ESP_LOGE(TAG, "Error writing POST body");
		goto cleanup;
	}

	free(postargs);
	postargs = NULL;


	int content_length = esp_http_client_fetch_headers(http_client);
	if(content_length == -1) {
		ESP_LOGE(TAG, "Error reading response headers");
		goto cleanup;
	}


	int status_code = esp_http_client_get_status_code(http_client);
	if(status_code >= 400) {
		ESP_LOGE(TAG, "HTTP response status code %d", status_code);
		goto cleanup;
	}

	ESP_LOGI(TAG, "HTTP response status code %d", status_code);


	read_loop(http_client);


cleanup:
	if(postargs) free(postargs);

	esp_http_client_close(http_client);
	esp_http_client_cleanup(http_client);
}


static void main_task(void *task_params)
{
	ESP_LOGI(TAG, "Hello world!");

	init_networking();


	do_stuff();



	printf("Spinning indefinitely...");
	fflush(stdout);

	while(1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

static void blink_task(void *task_params)
{
	gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_GPIO, 0);

    while(1) {
        if(xSemaphoreTake(blink_semaphore, 100) != pdTRUE) continue;

        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, 0);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}


void app_main()
{
	pre_task_init();

	xTaskCreate(&main_task, "main_task", 4096, NULL, 5, NULL);
	xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
}
