#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>

/**
 * @file led_indicator.h
 * @brief Interface for ZMK LED Indicator module
 *
 * This module provides unified LED feedback for:
 *  - Battery level changes
 *  - BLE connection state
 *  - Layer state transitions
 *
 * Automatically initialized via SYS_INIT() at application startup,
 * but manual reinitialization or refresh is possible via the API below.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB color structure for LED control
 */
struct led_rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/**
 * @brief Initialize LED indicator manually (optional)
 *
 * Normally initialized automatically at boot by SYS_INIT().
 * Returns 0 on success or negative error code on failure.
 */
int led_indicator_init(void);

/**
 * @brief Manually trigger LED indicator update
 *
 * Can be used to force a refresh after custom state changes.
 * Typically called automatically in response to ZMK events.
 */
void led_indicator_update(void);

#ifdef __cplusplus
}
#endif
