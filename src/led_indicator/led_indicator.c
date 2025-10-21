#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>
#include <zmk/keymap.h>
#include <zmk/layers.h>

#include "battery_monitor.h"

LOG_MODULE_REGISTER(led_indicator, LOG_LEVEL_INF);

/* === DeviceTreeノード取得 === */
#define LED_INDICATOR_NODE DT_INST(0, zmk_led_indicator)
#define STRIP_NODE DT_PHANDLE(LED_INDICATOR_NODE, led_strip)

#if DT_NODE_HAS_STATUS(STRIP_NODE, okay)
static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);
#else
#error "LED strip device not found"
#endif

/* === オプションGPIO LED === */
#if DT_NODE_HAS_PROP(LED_INDICATOR_NODE, gpio_leds)
#define GPIO_NODE DT_PHANDLE(LED_INDICATOR_NODE, gpio_leds)
static const struct device *gpio_dev = DEVICE_DT_GET(GPIO_NODE);
#define LED_EN_PIN 9
#define LED_PIN 10
#endif

static struct led_rgb color[1];

static void set_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    color[0].r = r;
    color[0].g = g;
    color[0].b = b;
    led_strip_update_rgb(strip, color, 1);
}

/* === 状態更新関数 === */
void led_indicator_update(void)
{
    int mv = battery_monitor_get_voltage_mv();

    if (mv < 0) {
        LOG_WRN("Battery voltage unavailable");
        return;
    }

    /* --- バッテリー状態 --- */
    if (mv < 3000) {
        set_led_color(255, 0, 0); // 赤
        k_sleep(K_MSEC(500));
        set_led_color(0, 0, 0);
        return;
    } else if (mv < 3300) {
        set_led_color(255, 165, 0); // オレンジ
    } else {
        set_led_color(0, 255, 0); // 緑
    }

    /* --- Bluetooth状態 --- */
    bool bt_ready = bt_is_ready();
    int conn_count = 0;
    if (bt_ready) {
        struct bt_conn *conns[CONFIG_BT_MAX_CONN];
        conn_count = bt_conn_get_connections(conns, CONFIG_BT_MAX_CONN);
    }

    if (conn_count == 0) {
        set_led_color(0, 0, 255); // 青点滅: 未接続
        k_sleep(K_MSEC(300));
        set_led_color(0, 0, 0);
        k_sleep(K_MSEC(300));
    }

    /* --- レイヤー状態 --- */
    uint8_t layer = zmk_keymap_layer_active();
    if (layer == 1) {
        set_led_color(0, 0, 255);
    } else if (layer == 2) {
        set_led_color(255, 255, 0);
    } else if (layer == 6) {
        set_led_color(0, 255, 0);
    } else {
        set_led_color(255, 255, 255);
    }

    k_sleep(K_MSEC(300));
    set_led_color(0, 0, 0);
}

/* === 初期化 === */
static int led_indicator_init(void)
{
    if (!device_is_ready(strip)) {
        LOG_ERR("LED strip not ready");
        return -ENODEV;
    }

#if DT_NODE_HAS_PROP(LED_INDICATOR_NODE, gpio_leds)
    if (device_is_ready(gpio_dev)) {
        gpio_pin_configure(gpio_dev, LED_EN_PIN, GPIO_OUTPUT);
        gpio_pin_configure(gpio_dev, LED_PIN, GPIO_OUTPUT);
        gpio_pin_set(gpio_dev, LED_EN_PIN, 1);
    }
#endif

    LOG_INF("LED Indicator initialized");
    return 0;
}

SYS_INIT(led_indicator_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
