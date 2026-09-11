/*
 * reed_sensor.c — debounced, monostable reed-switch detector.
 *
 * One REED_EVT_PASS per carriage pass, then the line must return HIGH and stay
 * HIGH for REED_REARM_MS before the next pass can be detected (a carriage
 * parked on the sensor produces exactly one event).
 */

#include "reed_sensor.h"
#include "board_config.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

typedef enum {
    ARMED,   // waiting for the next pass
    FIRED    // pass signalled; waiting for release + re-arm
} reed_state_t;

static reed_state_t s_state = ARMED;
static uint32_t s_low_start = 0;    // ms timestamp when the line first went low
static uint32_t s_high_start = 0;   // ms timestamp when the line first went high

void reed_sensor_init(void) {
    gpio_init(PIN_REED_SENSOR);
    gpio_set_dir(PIN_REED_SENSOR, GPIO_IN);
    gpio_pull_up(PIN_REED_SENSOR);
    gpio_set_input_enabled(PIN_REED_SENSOR, true);
}

reed_event_t reed_sensor_task(void) {
    // Line LOW == magnet present (reed closed to GND).
    bool low = !gpio_get(PIN_REED_SENSOR);
    uint32_t now = to_ms_since_boot(get_absolute_time());

    switch (s_state) {
        case ARMED:
            if (low) {
                if (s_low_start == 0) {
                    s_low_start = now;
                } else if (now - s_low_start >= REED_DEBOUNCE_MS) {
                    s_state = FIRED;
                    s_low_start = 0;
                    s_high_start = 0;
                    return REED_EVT_PASS;
                }
            } else {
                s_low_start = 0;   // not a stable low yet
            }
            break;

        case FIRED:
            if (low) {
                s_high_start = 0;  // carriage still parked on the sensor
            } else {
                if (s_high_start == 0) {
                    s_high_start = now;
                } else if (now - s_high_start >= REED_REARM_MS) {
                    s_state = ARMED;
                    s_high_start = 0;
                    return REED_EVT_RELEASE;
                }
            }
            break;
    }

    return REED_EVT_NONE;
}
