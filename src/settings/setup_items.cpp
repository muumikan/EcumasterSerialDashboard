#include "setup_items.hpp"

#include <stddef.h>

namespace ecu {
namespace {

#define LIMIT_OFFSET(alarm, field) \
    static_cast<uint16_t>(offsetof(DashSettings, alarms.limits) + \
                          static_cast<uint8_t>(AlarmId::alarm) * sizeof(AlarmLimits) + \
                          offsetof(AlarmLimits, field))

#define FIELD_OFFSET(field) static_cast<uint16_t>(offsetof(DashSettings, field))

// Behaviour of the alarm system itself, plus which alarms are live at all.
const SetupItem kAlarmItems[] = {
    { "Arm delay",  "s", SetupType::U8,   FIELD_OFFSET(alarms.armDelayS),        1, 0, 15, 0 },
    { "Hysteresis", "%", SetupType::U8,   FIELD_OFFSET(alarms.hysteresisPercent), 1, 0, 10, 0 },
    { "Oil P",      "",  SetupType::Bool, LIMIT_OFFSET(OilPressure, enabled),    0, 0, 1, 0 },
    { "Coolant",    "",  SetupType::Bool, LIMIT_OFFSET(Coolant, enabled),        0, 0, 1, 0 },
    { "Lean",       "",  SetupType::Bool, LIMIT_OFFSET(Lean, enabled),           0, 0, 1, 0 },
    { "Batt low",   "",  SetupType::Bool, LIMIT_OFFSET(BatteryLow, enabled),     0, 0, 1, 0 },
    { "Charge",     "",  SetupType::Bool, LIMIT_OFFSET(BatteryHigh, enabled),    0, 0, 1, 0 },
    { "Knock",      "",  SetupType::Bool, LIMIT_OFFSET(Knock, enabled),          0, 0, 1, 0 },
    { "Inj DC",     "",  SetupType::Bool, LIMIT_OFFSET(InjectorDuty, enabled),   0, 0, 1, 0 },
    { "Fuel P",     "",  SetupType::Bool, LIMIT_OFFSET(FuelPressure, enabled),   0, 0, 1, 0 },
    { "IAT",        "",  SetupType::Bool, LIMIT_OFFSET(IntakeAir, enabled),      0, 0, 1, 0 },
};

const SetupItem kLimitItems[] = {
    { "Oil P warn",   "bar", SetupType::Float, LIMIT_OFFSET(OilPressure, warn),  0.1f, 0.2f, 6.0f, 1 },
    { "Oil P crit",   "bar", SetupType::Float, LIMIT_OFFSET(OilPressure, crit),  0.1f, 0.2f, 6.0f, 1 },
    { "CLT warn",     "C",   SetupType::Float, LIMIT_OFFSET(Coolant, warn),      1.0f, 70,   140,  0 },
    { "CLT crit",     "C",   SetupType::Float, LIMIT_OFFSET(Coolant, crit),      1.0f, 70,   140,  0 },
    { "Lean warn",    "",    SetupType::Float, LIMIT_OFFSET(Lean, warn),         0.01f, 0.70f, 1.30f, 2 },
    { "Lean crit",    "",    SetupType::Float, LIMIT_OFFSET(Lean, crit),         0.01f, 0.70f, 1.30f, 2 },
    { "Batt warn",    "V",   SetupType::Float, LIMIT_OFFSET(BatteryLow, warn),   0.1f, 9.0f, 14.5f, 1 },
    { "Batt crit",    "V",   SetupType::Float, LIMIT_OFFSET(BatteryLow, crit),   0.1f, 9.0f, 14.5f, 1 },
    { "Charge warn",  "V",   SetupType::Float, LIMIT_OFFSET(BatteryHigh, warn),  0.1f, 13.0f, 18.0f, 1 },
    { "Charge crit",  "V",   SetupType::Float, LIMIT_OFFSET(BatteryHigh, crit),  0.1f, 13.0f, 18.0f, 1 },
    { "Knock warn",   "V",   SetupType::Float, LIMIT_OFFSET(Knock, warn),        0.1f, 0.2f, 5.0f, 1 },
    { "Knock crit",   "V",   SetupType::Float, LIMIT_OFFSET(Knock, crit),        0.1f, 0.2f, 5.0f, 1 },
    { "Inj DC warn",  "%",   SetupType::Float, LIMIT_OFFSET(InjectorDuty, warn), 1.0f, 40,   100,  0 },
    { "Inj DC crit",  "%",   SetupType::Float, LIMIT_OFFSET(InjectorDuty, crit), 1.0f, 40,   100,  0 },
    { "Fuel P warn",  "bar", SetupType::Float, LIMIT_OFFSET(FuelPressure, warn), 0.1f, 1.0f, 8.0f, 1 },
    { "Fuel P crit",  "bar", SetupType::Float, LIMIT_OFFSET(FuelPressure, crit), 0.1f, 1.0f, 8.0f, 1 },
    { "IAT warn",     "C",   SetupType::Float, LIMIT_OFFSET(IntakeAir, warn),    1.0f, 30,   130,  0 },
    { "IAT crit",     "C",   SetupType::Float, LIMIT_OFFSET(IntakeAir, crit),    1.0f, 30,   130,  0 },
};

const SetupItem kShiftItems[] = {
    { "First light", "rpm", SetupType::U16, FIELD_OFFSET(shiftFirstRpm), 100, 1000, 9000, 0 },
    { "Red zone",    "rpm", SetupType::U16, FIELD_OFFSET(shiftRedRpm),   100, 1000, 9500, 0 },
    { "All lit",     "rpm", SetupType::U16, FIELD_OFFSET(shiftAllRpm),   100, 1000, 9500, 0 },
};

const SetupItem kDisplayItems[] = {
    { "Brightness",  "%", SetupType::U8,   FIELD_OFFSET(brightnessPct),      5, 10, 100, 0 },
    { "Night mode",  "",  SetupType::Bool, FIELD_OFFSET(nightMode),          0, 0, 1, 0 },
    { "Night level", "%", SetupType::U8,   FIELD_OFFSET(nightBrightnessPct), 5, 5,  100, 0 },
    { "Boot sweep",  "",  SetupType::Bool, FIELD_OFFSET(bootSweep),          0, 0, 1, 0 },
};

const SetupItem kLogItems[] = {
    { "Logging",     "", SetupType::Bool, FIELD_OFFSET(logging),    0, 0, 1, 0 },
    { "Service AP",  "", SetupType::Bool, FIELD_OFFSET(serviceAp),  0, 0, 1, 0 },
};

#define COUNT_OF(a) static_cast<uint8_t>(sizeof(a) / sizeof((a)[0]))

}  // namespace

const SetupCategory kSetupCategories[] = {
    { "Alarms",  kAlarmItems,   COUNT_OF(kAlarmItems) },
    { "Limits",  kLimitItems,   COUNT_OF(kLimitItems) },
    { "Shift",   kShiftItems,   COUNT_OF(kShiftItems) },
    { "Display", kDisplayItems, COUNT_OF(kDisplayItems) },
    { "Log",     kLogItems,     COUNT_OF(kLogItems) },
};

const uint8_t kSetupCategoryCount = COUNT_OF(kSetupCategories);

float readSetting(const DashSettings& settings, const SetupItem& item) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&settings) + item.offset;

    switch (item.type) {
        case SetupType::Float: return *reinterpret_cast<const float*>(base);
        case SetupType::U16:   return static_cast<float>(*reinterpret_cast<const uint16_t*>(base));
        case SetupType::U8:    return static_cast<float>(*base);
        case SetupType::Bool:  return *reinterpret_cast<const bool*>(base) ? 1.0f : 0.0f;
    }
    return 0.0f;
}

void writeSetting(DashSettings& settings, const SetupItem& item, float value) {
    uint8_t* base = reinterpret_cast<uint8_t*>(&settings) + item.offset;

    switch (item.type) {
        case SetupType::Float: *reinterpret_cast<float*>(base) = value; break;
        case SetupType::U16:   *reinterpret_cast<uint16_t*>(base) = static_cast<uint16_t>(value + 0.5f); break;
        case SetupType::U8:    *base = static_cast<uint8_t>(value + 0.5f); break;
        case SetupType::Bool:  *reinterpret_cast<bool*>(base) = value > 0.5f; break;
    }
}

float clampSetting(const SetupItem& item, float value) {
    if (item.type == SetupType::Bool) {
        return value > 0.5f ? 1.0f : 0.0f;
    }
    if (value < item.minValue) return item.minValue;
    if (value > item.maxValue) return item.maxValue;
    return value;
}

}  // namespace ecu
