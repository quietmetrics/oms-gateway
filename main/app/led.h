// Simple status LED HAL for GPIO8 using LEDC PWM.
#pragma once

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

#define STATUS_LED_GPIO GPIO_NUM_8
#define STATUS_LED_ACTIVE_LOW true

typedef enum
{
    STATUS_LED_PATTERN_OFF = 0,
    STATUS_LED_PATTERN_ON,
    STATUS_LED_PATTERN_BLINK_SLOW,
    STATUS_LED_PATTERN_BLINK_FAST,
    STATUS_LED_PATTERN_FADE_SLOW,
    STATUS_LED_PATTERN_DOUBLE_BLINK,
    STATUS_LED_PATTERN_PULSE_SHORT,
} status_led_pattern_t;

// Initialize LEDC on the given GPIO and start background task. inverted=true for active-low LEDs.
esp_err_t status_led_init(gpio_num_t gpio, bool inverted);

// Set the baseline pattern (persists until changed).
void status_led_set_base(status_led_pattern_t pattern);

// Trigger a one-shot pattern; LED returns to current base afterwards.
void status_led_trigger_once(status_led_pattern_t pattern);

// Convenience helpers for common actions.
static inline void status_led_on(void) { status_led_set_base(STATUS_LED_PATTERN_ON); }
static inline void status_led_off(void) { status_led_set_base(STATUS_LED_PATTERN_OFF); }
static inline void status_led_blink_slow(void) { status_led_set_base(STATUS_LED_PATTERN_BLINK_SLOW); }
static inline void status_led_blink_fast(void) { status_led_set_base(STATUS_LED_PATTERN_BLINK_FAST); }
static inline void status_led_fade_slow(void) { status_led_set_base(STATUS_LED_PATTERN_FADE_SLOW); }

// Short pulse used for packet reception indication.
static inline void status_led_pulse(void) { status_led_trigger_once(STATUS_LED_PATTERN_PULSE_SHORT); }
