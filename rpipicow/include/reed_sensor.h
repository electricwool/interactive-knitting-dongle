#pragma once
/*
 * reed_sensor.h — carriage-pass reed switch detector.
 *
 * Reed switch wired between GP27 and GND with the internal pull-up enabled, so
 * the line idles HIGH and a magnet closes the reed to pull it LOW. Debounced
 * with a monostable (one event per pass; re-arms only after the line has gone
 * HIGH again).
 */

#include <stdbool.h>

typedef enum {
    REED_EVT_NONE = 0,
    REED_EVT_PASS,      // magnet present (debounced LOW) — a carriage passed
    REED_EVT_RELEASE    // magnet gone and re-armed
} reed_event_t;

void reed_sensor_init(void);
reed_event_t reed_sensor_task(void);
