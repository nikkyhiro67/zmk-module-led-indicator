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

/* ==========================================================
 *  DeviceTree ノード定義
 * ========================================================== */
#define LED_INDICATOR_NODE DT_INST(0, zmk_led_indicator)
#define LED_STRIP_NODE     DT_PHANDLE(LED_INDICATOR_NODE, led_strip)
#define BATTERY_NODE       DT_PHANDLE_OR(LED_INDICATOR_NODE, battery, DT_INVALID_NODE)

static const struct device *led_strip_dev = DEVICE_DT_GET(LED_STRIP_NODE);
#if DT_NODE_HAS_PROP(LED_INDICATOR_NODE, battery)
static const struct device *battery_dev = DEVICE_DT_GET(BATTERY_NODE);
#else
static const struct device *battery_dev = NULL;
#endif

/* ==========================================================
 *  状態変数
 * ========================================================== */
static struct led_rgb led_color = {0, 0, 0};
static uint8_t active_layer = 0;
static bool ble_connected = false;
static uint8_t battery_level = 100;

/* ==========================================================
 *  バッテリー残量に応じた色変換
 * ========================================================== */
static struct led_rgb get_battery_color(uint8_t level, bool connected)
{
    struct led_rgb color = {0};

    if (level >= 80) {
        // 高残量：緑（接続中は少し青寄り）
        color.r = 0x00;
        color.g = 0xFF;
        color.b = connected ? 0x40 : 0x00;
    } else if (level >= 50) {
        // 中残量：オレンジ（接続中はわずかに青）
        color.r = 0xFF;
        color.g = 0xA5;
        color.b = connected ? 0x20 : 0x00;
    } else {
        // 低残量：赤（接続中はわずかに青）
        color.r = 0xFF;
        color.g = 0x00;
        color.b = connected ? 0x20 : 0x00;
    }

    return color;
}

/* ==========================================================
 *  LED更新処理
 * ========================================================== */
static void led_indicator_set_color(uint8_t r, uint8_t g, uint8_t b)
{
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

/* ==========================================================
 *  状態に応じたLED制御
 * ========================================================== */
static void led_indicator_update(void)
{
    struct led_rgb color = get_battery_color(battery_level, ble_connected);

    /* レイヤーに応じた上書き（例：Fn層＝青） */
    if (active_layer == 1) {
        color.r = 0x00;
        color.g = 0x00;
        color.b = 0xFF;
    }

    led_indicator_set_color(color.r, color.g, color.b);
}

/* ==========================================================
 *  イベントリスナー
 * ========================================================== */
static int led_indicator_listener(const struct zmk_event_header *eh)
{
    if (is_zmk_layer_state_changed(eh)) {
        const struct zmk_layer_state_changed *ev = cast_zmk_layer_state_changed(eh);
        active_layer = ev->state;
        LOG_DBG("Layer changed: %d", active_layer);
        led_indicator_update();

    } else if (is_zmk_ble_connection_status_changed(eh)) {
        const struct zmk_ble_connection_status_changed *ev = cast_zmk_ble_connection_status_changed(eh);
        ble_connected = ev->connected;
        LOG_INF("BLE connection status: %s", ble_connected ? "connected" : "disconnected");
        led_indicator_update();

    } else if (is_zmk_battery_state_changed(eh)) {
        const struct zmk_battery_state_changed *ev = cast_zmk_battery_state_changed(eh);
        battery_level = ev->state_of_charge;
        LOG_INF("Battery level changed: %d%%", battery_level);
        led_indicator_update();
    }

    return 0;
}

/* ==========================================================
 *  ZMKイベント登録
 * ========================================================== */
ZMK_LISTENER(led_indicator, led_indicator_listener);
ZMK_SUBSCRIPTION(led_indicator, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(led_indicator, zmk_ble_connection_status_changed);
ZMK_SUBSCRIPTION(led_indicator, zmk_battery_state_changed);

/* ==========================================================
 *  初期化関数
 * ========================================================== */
int led_indicator_init(void)
{
    if (!device_is_ready(led_strip_dev)) {
        LOG_ERR("LED strip device not ready");
        return -ENODEV;
    }

    LOG_INF("LED Indicator initialized (linked to BLE, Layer, and Battery state)");
    led_indicator_update();
    return 0;
}

SYS_INIT(led_indicator_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
