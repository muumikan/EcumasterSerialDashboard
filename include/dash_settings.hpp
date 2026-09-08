#pragma once

#include <stdint.h>

#include "alarm_engine.hpp"

namespace ecu {

// Everything the driver can change from the setup page, and nothing else.
//
// Only numbers and on/off flags live here. Which value an alarm watches and
// which way it trips stays in code: a settings page that can rewire logic is a
// settings page that can brick the dash on a dark road.
struct DashSettings {
    AlarmSettings alarms;

    uint16_t shiftFirstRpm;   // first segment lights
    uint16_t shiftRedRpm;     // segments turn red from here
    uint16_t shiftAllRpm;     // every segment lit

    uint8_t brightnessPct;       // 10..100
    uint8_t nightBrightnessPct;  // used while night mode is on
    bool nightMode;

    uint16_t idleReturnS;   // back to Drive after this long untouched; 0 = never
    bool bootSweep;
    bool logging;
};

DashSettings defaultDashSettings();

}  // namespace ecu
