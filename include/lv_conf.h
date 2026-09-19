// Minimal LVGL 8.3 configuration for the CrowPanel Advance 3.5.
// Anything not set here falls back to the defaults in lv_conf_internal.h.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// Every widget on every page comes out of this one fixed array - eight pages,
// all built at boot and kept. It is not the general heap and does not grow.
//
// Running it out is worth understanding, because it does not look like running
// out of memory: lv_obj_create returns NULL, LVGL dereferences it one call
// later without an assert, and the dashboard panics and reboots with nothing
// printed. That is what building the SETUP page's eighteen Limits rows did at
// 96 kB, once the IDLE and BOOST pages had taken their share.
//
// The page that caused it now works from a fixed pool of rows, so nothing
// left grows with the settings table. This is the margin on top of that, and
// the service page reports what is left of it under Diagnostics so the next
// page added is not a guess. 32 kB more static RAM, out of about 170 kB that
// was free.
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128U * 1024U)

#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 20

// Drive LVGL's clock straight from millis() so no extra timer is needed.
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_BUILD_EXAMPLES 0

#endif  // LV_CONF_H
