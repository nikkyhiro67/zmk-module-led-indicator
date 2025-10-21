#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/ble_connection_status_changed.h>

LOG_MODULE_REGISTER(led_indicator, CONFIG_ZMK_LOG_LEVEL);

#define LED_INDICATOR_NODE DT_INST(0, zmk_led_indicator)
#define LED_STRIP_NODE     DT_PHANDLE(LED_INDICATOR_NODE, led_strip)
#define BATTERY_NODE       DT_PHANDLE_OR(LED_INDICATOR_NODE, battery, DT_INVALID_NODE)

static const struct device *led_strip_dev = DEVICE_DT_GET(LED_STRIP_NODE);
#if DT_NODE_HAS_PROP(LED_INDICATOR_NODE, battery)
static const struct device *battery_dev = DEVICE_DT_GET(BATTERY_NODE);
#else
static const struct device *battery_dev = NULL;
#endif

static struct led_rgb led_color = {0, 0, 0};
static uint8_t active_layer = 0;
static bool ble_connected = false;
static uint8_t battery_level = 100;

static void led_indicator_set_color(uint8_t r, uint8_t g, uint8_t b) {
    led_color.r = r;
    led_color.g = g;
    led_color.b = b;

    if (!device_is_ready(led_strip_dev)) {
        LOG_ERR("LED strip device not ready");
        return;
    }

    int ret = led_strip_update_rgb(led_strip_dev, &led_color, 1);
    if (ret) {
        LOG_ERR("Failed to update LED strip: %d", ret);
    }
}

void led_indicator_update(void) {
    uint8_t level = battery_level;

    if (ble_connected) {
        led_indicator_set_color(0, 0, level * 255 / 100);
    } else {
        led_indicator_set_color(level * 255 / 100, 0, 0);
    }
}

static int led_indicator_listener(const struct zmk_event_header *eh) {
    if (is_zmk_layer_state_changed(eh)) {
        const struct zmk_layer_state_changed *ev = cast_zmk_layer_state_changed(eh);
        active_layer = ev->state;
        led_indicator_update();
    } else if (is_zmk_ble_connection_status_changed(eh)) {
        const struct zmk_ble_connection_status_changed *ev = cast_zmk_ble_connection_status_changed(eh);
        ble_connected = ev->connected;
        led_indicator_update();
    } else if (is_zmk_battery_state_changed(eh)) {
        const struct zmk_battery_state_changed *ev = cast_zmk_battery_state_changed(eh);
        battery_level = ev->state_of_charge;
        led_indicator_update();
    }
    return 0;
}

ZMK_LISTENER(led_indicator, led_indicator_listener);
ZMK_SUBSCRIPTION(led_indicator, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(led_indicator, zmk_ble_connection_status_changed);
ZMK_SUBSCRIPTION(led_indicator, zmk_battery_state_changed);

int led_indicator_init(void) {
    if (!device_is_ready(led_strip_dev)) {
        LOG_ERR("LED strip device not ready");
        return -ENODEV;
    }

    LOG_INF("LED Indicator initialized");
    led_indicator_update();
    return 0;
}

SYS_INIT(led_indicator_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
