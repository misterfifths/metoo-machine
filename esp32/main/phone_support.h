// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include "sdkconfig.h"

#if CONFIG_TARGET_PHONE && !defined(_PHONE_SUPPORT_H)
#define _PHONE_SUPPORT_H


#include "app_task.h"


extern const app_task_descriptor phone_task_descriptor;
extern const app_task_descriptor phone_handset_led_task_descriptor;


typedef enum {
	phone_audio_target_mute = 0,
	phone_audio_target_speaker = 1 << 0,
	phone_audio_target_handset = 1 << 1,
	phone_audio_target_both = phone_audio_target_speaker | phone_audio_target_handset
} phone_audio_target;

// Set the destination of the audio from the amplifier.
// Specify one or more phone_audio_target values or-ed together
void phone_set_audio_target(phone_audio_target target);

// Control the LED that shows through the handset.
void phone_handset_led_set(bool on);
void phone_handset_led_blink(void);  // asynchronous; call one of the stop methods to end it
void phone_handset_led_blink_from_ISR(void);
void phone_handset_led_blink_stop(void);
void phone_handset_led_blink_stop_from_ISR(void);

// Control the status LED (i.e., the speakerphone button indicator)
void phone_status_led_set(bool on);
void phone_status_led_flash(uint8_t flash_count);  // synchronous; will delay the calling task ~200ms * flash_count

// Returns true if the phone handset is on the hook.
bool phone_is_handset_on_hook(void);


#endif
