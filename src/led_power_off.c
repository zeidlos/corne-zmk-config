/*
 * Keeps the LED power rail (EXT_POWER) off on a half whose LEDs are unused.
 *
 * ZMK's ext-power driver switches the rail on at boot, and with underglow
 * disabled nothing switches it off again. Dark WS2812s still draw ~1 mA each,
 * which empties the battery in a day or two.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/ext_power.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static void power_off(struct k_work *work) {
    const struct device *ext_power = device_get_binding("EXT_POWER");
    if (!ext_power) {
        LOG_ERR("led_power_off: no EXT_POWER device");
        return;
    }
    ext_power_disable(ext_power);
}

static K_WORK_DELAYABLE_DEFINE(power_off_work, power_off);

static int led_power_off_init(void) {
    /* Delayed past main()'s settings_load(), which restores a saved state. */
    k_work_schedule(&power_off_work, K_MSEC(1000));
    return 0;
}

SYS_INIT(led_power_off_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
