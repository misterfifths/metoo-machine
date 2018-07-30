// 2018 / Tim Clem / github.com/misterfifths
// Partially adapted from Espressif ESP IDF sample code.
// Public domain.

#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "app_wifi.h"
#include "app_wifi_config.h"

// TODO: eap_i.h is internal, but needed to get at eap_sm guts (via eap_get_config).
// :( Should PR something upstream to support password hashes
#include "wpa/common.h"
#include "wpa2/eap_peer/eap_config.h"
#include "wpa2/eap_peer/eap_i.h"


static const char *TAG = "WIFI";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group = NULL;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

// Used to customize initialization to support hashed passwords
static wpa2_crypto_funcs_t *app_wifi_crypto_funcs = NULL;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
        	ESP_LOGI(TAG, "Connecting to wifi...");
        	ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
        	ESP_LOGI(TAG, "Wifi connected. IP is %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
               auto-reassociate. */
        	ESP_LOGI(TAG, "Wifi disconnected. Reassociating.");
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}


#if CONFIG_WIFI_AUTH_MODE == _CONFIG_WIFI_AUTH_MODE_WPA2_EAP && CONFIG_WIFI_PASSWORD_IS_HASH == 1

static int app_wifi_eap_peer_config_init_hashed(void *sm, unsigned char *private_key_passwd, int private_key_passwd_len)
{
	int res = eap_peer_config_init(sm, private_key_passwd, private_key_passwd_len);
	if(res != 0) return res;  // Propagate any errors

	struct eap_peer_config *peer_config = eap_get_config(sm);
	peer_config->flags |= EAP_CONFIG_FLAGS_PASSWORD_NTHASH;

	return 0;
}

static esp_err_t app_wifi_sta_wpa2_ent_set_password_hash(const char *password_hash)
{
	if(strlen(password_hash) != 32) return ESP_ERR_INVALID_ARG;

	uint8_t buf[16] = { 0 };
	int res = hexstr2bin(password_hash, buf, 16);  // length here is in bytes, 16 bytes = 32 chars
	if(res != 0) return ESP_ERR_INVALID_ARG;

	return esp_wifi_sta_wpa2_ent_set_password(buf, 16);
}

#endif


void app_wifi_initialize(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,

#if CONFIG_WIFI_AUTH_MODE == _CONFIG_WIFI_AUTH_MODE_WPA
            .password = CONFIG_WIFI_PASSWORD
#endif
        }
    };

    ESP_LOGI(TAG, "Configuring wifi for SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));


#if CONFIG_WIFI_AUTH_MODE == _CONFIG_WIFI_AUTH_MODE_WPA2_EAP

    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();

	#if CONFIG_WIFI_PASSWORD_IS_HASH == 1
        // Install our custom eap_peer_config_init hook that sets the appropriate flag
		app_wifi_crypto_funcs = malloc(sizeof(wpa2_crypto_funcs_t));
		memcpy(app_wifi_crypto_funcs, config.crypto_funcs, sizeof(wpa2_crypto_funcs_t));
		app_wifi_crypto_funcs->eap_peer_config_init = app_wifi_eap_peer_config_init_hashed;
		config.crypto_funcs = app_wifi_crypto_funcs;

		// And call our custom function that decodes the hex password string
		ESP_ERROR_CHECK(app_wifi_sta_wpa2_ent_set_password_hash(CONFIG_WIFI_PASSWORD));
	#else
		const size_t password_len = strlen(CONFIG_WIFI_PASSWORD);
		ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_password((const unsigned char *)CONFIG_WIFI_PASSWORD, password_len));
	#endif

	const size_t identity_len = strlen(CONFIG_WIFI_EAP_IDENTITY);
	if(identity_len != 0) {
		ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_identity((const unsigned char *)CONFIG_WIFI_EAP_IDENTITY, identity_len));
	}

	const size_t username_len = strlen(CONFIG_WIFI_USERNAME);
	if(username_len != 0) {
		ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_username((const unsigned char *)CONFIG_WIFI_USERNAME, username_len));
	}

	ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_enable(&config));
#endif


    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_wifi_wait_connected()
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

void app_wifi_stop()
{
	// Uninstall the event handler so it doesn't try to reconnect
	esp_event_loop_set_cb(NULL, NULL);

	if(wifi_event_group) {
		vEventGroupDelete(wifi_event_group);
		esp_wifi_disconnect();
		esp_wifi_stop();
	}

#if CONFIG_WIFI_AUTH_MODE == _CONFIG_WIFI_AUTH_MODE_WPA2_EAP
	esp_wifi_sta_wpa2_ent_disable();
#endif

	if(app_wifi_crypto_funcs) {
		free(app_wifi_crypto_funcs);
		app_wifi_crypto_funcs = NULL;
	}
}
