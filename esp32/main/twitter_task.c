// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_http_client.h"

#include "cJSON.h"
#include "oauth.h"

#include "twitter_task.h"
#include "audio_task.h"
#include "secrets.h"
#include "rolling_buffer.h"


const uint32_t twitter_task_stack_size = 4 * 1024;

static const char *TAG = "TWT";

static uint32_t retry_delay_ms = 0;

typedef enum {
	twitter_error_networking,  // TODO: this catches TCP/IP stuff as well as connectivity issues
	twitter_error_general_http,  // 400 & 500 HTTP statuses
	twitter_error_http_420  // 420 status, Twitter's (very adult) rate limit
} twitter_error;


const uint32_t json_buffer_length = 20 * 1024;
static rbuf *json_buffer;


static void handle_tweet(cJSON *json)
{
	audio_task_enqueue_sound(audio_task_sound_tweet);
}


static twitter_error read_loop(esp_http_client_handle_t http_client)
{
	rbuf_reset(json_buffer);

	while(1) {
		char *next_read_location;
		size_t next_read_length;
		rbuf_get_write_info(json_buffer, &next_read_location, &next_read_length);
		ESP_LOGD(TAG, "Reading %zu bytes of the response", next_read_length);

		// TODO: esp_http_client_read blocks until it reads the number of bytes we request.
		// This means we can stall until we get enough tweets to fill the buffer.
		// The alternative is to request a smaller number of bytes and handle the case of not
		// having a valid JSON document. The docs are unclear on this, but the Tweepy
		// source (https://github.com/tweepy/tweepy/blob/master/tweepy/streaming.py) implies
		// that messages are separated by newlines. So the thing to do would be read a small
		// number of bytes (1024?), and if we got a newline, try to parse it as JSON. If we
		// didn't get a newline, read more. If the buffer is full and we haven't seen a newline,
		// then the incoming tweet data is too big for our buffer and we're screwed.

		int bytes_read = esp_http_client_read(http_client, next_read_location, next_read_length);
		if(bytes_read == -1) {
			ESP_LOGE(TAG, "Error reading response body");
			goto cleanup;
		}

		rbuf_add_bytes(json_buffer, bytes_read);

		ESP_LOGD(TAG, "Read %d bytes; there are %zu valid bytes in the buffer", bytes_read, rbuf_get_valid_byte_count(json_buffer));


		const char *end_of_json_document = NULL;
		cJSON *json = cJSON_ParseWithOpts(rbuf_get_bytes(json_buffer), &end_of_json_document, 0);
		if(json == NULL) {
			// TODO: this might happen if we are unable to fit an entire tweet within the buffer
			// Hoping to mitigate that by just making the buffer sufficiently large.... risky business
			size_t error_offset = cJSON_GetErrorPtr() - rbuf_get_bytes(json_buffer);
			ESP_LOGE(TAG, "Unable to parse response as JSON (starting at character %zu): %s", error_offset, rbuf_get_bytes(json_buffer));
			goto cleanup;
		}


		// We get a variety of messages through this channel (disconnects, rate limits, user updates, etc.).
		// Tweets seem to be the only one with a "text" property, so I'm using that as the discriminator.
		// TODO: make note of disconnect messages, stall notifications, and limit messages?
		if(cJSON_HasObjectItem(json, "text")) {
			ESP_LOGI(TAG, "a tweet!");
			handle_tweet(json);
		}
		else {
			char *jsonString = cJSON_Print(json);
			ESP_LOGI(TAG, "Something other than a tweet!\n%s", jsonString);
			free(jsonString);
		}

		cJSON_Delete(json);

		rbuf_discard_bytes_ending_at(json_buffer, end_of_json_document);
		ESP_LOGD(TAG, "There are now %zu bytes left in the buffer", rbuf_get_valid_byte_count(json_buffer));
	}

cleanup:
	// Making the safe-seeming assumption that trouble at this point is a networking error of some sort
	return twitter_error_networking;
}


static twitter_error connect_to_twitter(void)
{
	const char *url = "https://stream.twitter.com/1.1/statuses/filter.json";

	twitter_error res = twitter_error_networking;

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


	ESP_LOGI(TAG, "Opening HTTP connection...");

	esp_err_t err;
	err = esp_http_client_open(http_client, postargs_length);
	if(err != ESP_OK) {
		res = twitter_error_networking;
		ESP_LOGE(TAG, "Error opening connection: %s", esp_err_to_name(err));
		goto cleanup;
	}


	int bytes_written = esp_http_client_write(http_client, postargs, postargs_length);
	if(bytes_written == -1) {
		res = twitter_error_networking;
		ESP_LOGE(TAG, "Error writing POST body");
		goto cleanup;
	}

	free(postargs);
	postargs = NULL;


	int content_length = esp_http_client_fetch_headers(http_client);
	if(content_length == -1) {
		res = twitter_error_networking;
		ESP_LOGE(TAG, "Error reading response headers");
		goto cleanup;
	}


	int status_code = esp_http_client_get_status_code(http_client);
	if(status_code >= 400) {
		res = status_code == 420 ? twitter_error_http_420 : twitter_error_general_http;
		ESP_LOGE(TAG, "HTTP response status code %d", status_code);
		goto cleanup;
	}

	// Wipe any retry backoff from previous errors
	retry_delay_ms = 0;

	audio_task_enqueue_sound(audio_task_sound_success3);
	ESP_LOGI(TAG, "HTTP response status code %d. Entering read loop", status_code);


	res = read_loop(http_client);


cleanup:
	if(postargs) free(postargs);

	esp_http_client_close(http_client);
	esp_http_client_cleanup(http_client);

	return res;
}


void twitter_task_main(void *task_params)
{
	// The ESP HTTP client log level is debug by default, and it is *chatty*
	esp_log_level_set("HTTP_CLIENT", ESP_LOG_INFO);

	json_buffer = rbuf_alloc(json_buffer_length);


	while(1) {
		twitter_error err = connect_to_twitter();


		// If connect_to_twitter returns, it means we hit an error.
		audio_task_enqueue_sound(audio_task_sound_error);


		// We'll be backing off on retries as the API docs suggest:
		// https://developer.twitter.com/en/docs/tweets/filter-realtime/guides/connecting
		if(err == twitter_error_networking) {
			// "Back off linearly for TCP/IP level network errors. These problems are generally temporary
			// and tend to clear quickly. Increase the delay in reconnects by 250ms each attempt, up to 16 seconds"
			retry_delay_ms += 250;

			if(retry_delay_ms > 16 * 1000) retry_delay_ms = 16 * 1000;
		}
		else if(err == twitter_error_general_http) {
			// "Back off exponentially for HTTP errors for which reconnecting would be appropriate. Start
			// with a 5 second wait, doubling each attempt, up to 320 seconds."
			if(retry_delay_ms == 0) retry_delay_ms = 5 * 1000;
			else retry_delay_ms *= 2;

			if(retry_delay_ms > 320 * 1000) retry_delay_ms = 320 * 1000;
		}
		else if(err == twitter_error_http_420) {
			// "Back off exponentially for HTTP 420 errors. Start with a 1 minute wait and double each
			// attempt. Note that every HTTP 420 received increases the time you must wait until rate
			// limiting will no longer will be in effect for your account."
			// We shouldn't see any of these, but you never know.
			if(retry_delay_ms == 0) retry_delay_ms = 60 * 1000;
			else retry_delay_ms *= 2;
		}

		ESP_LOGI(TAG, "Twitter error. Retrying in %u ms", retry_delay_ms);
		vTaskDelay(retry_delay_ms / portTICK_PERIOD_MS);
	}
}
