#pragma once

#include <stdint.h>

// Display + touch + LVGL bring-up for the CrowPanel Advance 3.5.
//
// Keeps LovyanGFX and the panel wiring out of the screen code: the screens
// only ever talk to LVGL.
namespace display {

// Brings up the SPI panel, the GT911 touch controller and LVGL itself.
// Returns false if the LVGL draw buffers could not be allocated.
bool begin();

// Runs LVGL's task handler. Call from the main loop.
void loop();

}  // namespace display
