/*
 * Per-key BLE status LEDs.
 *
 * Drives two LEDs of the underglow strip directly (ZMK's own underglow must be
 * off): one shows the active BLE profile as a color, the other the profile's
 * state: green = connected, red = paired but not connected, blue = no pairing
 * (open, advertising for a new host). All LEDs go dark when the keyboard goes
 * idle.
 *
 * With CONFIG_ZMK_STATUS_LEDS_CALIBRATE every LED shows its own chain index
 * instead: color = index / 5 (red, green, blue, yellow, white), number of
 * blinks per cycle = index % 5 + 1.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/ext_power.h>
#include <zmk/activity.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_NODE DT_CHOSEN(zmk_underglow)
#define NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)
#define BRT CONFIG_ZMK_STATUS_LEDS_BRIGHTNESS

BUILD_ASSERT(CONFIG_ZMK_STATUS_LEDS_PROFILE_INDEX < NUM_PIXELS, "profile LED index out of range");
BUILD_ASSERT(CONFIG_ZMK_STATUS_LEDS_CONN_INDEX < NUM_PIXELS, "connection LED index out of range");

static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixels[NUM_PIXELS];

#define RGB(R, G, B) ((struct led_rgb){.r = (R), .g = (G), .b = (B)})

static void flush(void) {
    int err = led_strip_update_rgb(strip, pixels, NUM_PIXELS);
    if (err) {
        LOG_ERR("status_leds: strip update failed: %d", err);
    }
}

static void clear(void) { memset(pixels, 0, sizeof(pixels)); }

#if IS_ENABLED(CONFIG_ZMK_STATUS_LEDS_CALIBRATE)

/* 300 ms per step; up to 5 blinks (10 steps) then a 5-step pause. */
#define CAL_STEP_MS 300
#define CAL_STEPS 15

static const struct led_rgb cal_colors[] = {
    RGB(BRT, 0, 0), RGB(0, BRT, 0), RGB(0, 0, BRT), RGB(BRT, BRT, 0), RGB(BRT, BRT, BRT),
};

static void cal_tick(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cal_work, cal_tick);
static int cal_step;

static void cal_tick(struct k_work *work) {
    clear();
    for (int i = 0; i < NUM_PIXELS; i++) {
        int blinks = i % 5 + 1;
        if (cal_step < 2 * blinks && cal_step % 2 == 0) {
            pixels[i] = cal_colors[(i / 5) % ARRAY_SIZE(cal_colors)];
        }
    }
    flush();
    cal_step = (cal_step + 1) % CAL_STEPS;
    k_work_schedule(&cal_work, K_MSEC(CAL_STEP_MS));
}

#else /* normal mode */

/* Profiles 1-5. No red or green, those belong to the connection LED (blue is
 * shared, but on a different key). */
static const struct led_rgb profile_colors[] = {
    RGB(BRT, BRT, BRT),     /* 1 white */
    RGB(0, 0, BRT),         /* 2 blue */
    RGB(BRT, BRT * 2 / 3, 0), /* 3 yellow */
    RGB(BRT, 0, BRT),       /* 4 magenta */
    RGB(0, BRT, BRT),       /* 5 cyan */
};

static bool active = true;

static void refresh(struct k_work *work) {
    clear();
    if (active) {
        int profile = zmk_ble_active_profile_index();
        if (profile >= 0) {
            pixels[CONFIG_ZMK_STATUS_LEDS_PROFILE_INDEX] =
                profile_colors[profile % ARRAY_SIZE(profile_colors)];
        }
        struct led_rgb conn;
        if (zmk_ble_active_profile_is_connected()) {
            conn = RGB(0, BRT, 0);
        } else if (zmk_ble_active_profile_is_open()) {
            conn = RGB(0, 0, BRT);
        } else {
            conn = RGB(BRT, 0, 0);
        }
        pixels[CONFIG_ZMK_STATUS_LEDS_CONN_INDEX] = conn;
    }
    flush();
}

static K_WORK_DEFINE(refresh_work, refresh);

static int status_leds_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev) {
        active = ev->state == ZMK_ACTIVITY_ACTIVE;
    }
    k_work_submit(&refresh_work);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(status_leds, status_leds_listener);
ZMK_SUBSCRIPTION(status_leds, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(status_leds, zmk_activity_state_changed);

#endif

static int status_leds_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("status_leds: LED strip not ready");
        return -ENODEV;
    }

#if IS_ENABLED(CONFIG_ZMK_EXT_POWER)
    /* The LEDs sit on the switchable rail; underglow used to turn it on. */
    const struct device *ext_power = device_get_binding("EXT_POWER");
    if (ext_power) {
        ext_power_enable(ext_power);
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_STATUS_LEDS_CALIBRATE)
    k_work_schedule(&cal_work, K_MSEC(500));
#else
    k_work_submit(&refresh_work);
#endif
    return 0;
}

SYS_INIT(status_leds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
