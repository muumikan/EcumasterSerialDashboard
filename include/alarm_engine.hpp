#pragma once

#include <stdint.h>

#include "engine_data.hpp"

namespace ecu {

enum class AlarmSeverity : uint8_t {
    None = 0,
    Warning = 1,
    Critical = 2,
};

enum class AlarmId : uint8_t {
    OilPressure,
    Coolant,
    Lean,
    BatteryLow,
    BatteryHigh,
    Knock,
    InjectorDuty,
    FuelPressure,
    IntakeAir,
    Count,
};

constexpr uint8_t kAlarmCount = static_cast<uint8_t>(AlarmId::Count);

// Below this the engine is not turning, so oil pressure, battery voltage and
// everything else that is only meaningful under load stays silent.
constexpr uint16_t kEngineRunningRpm = 500;

// Limits for one alarm. Held as data rather than baked into the comparison, so
// the setup page can reach them without a reflash.
struct AlarmLimits {
    float warn;
    float crit;
    bool enabled;
};

struct AlarmSettings {
    AlarmLimits limits[kAlarmCount];

    // The engine has to have been running this long before anything can trip.
    // Cranking crosses 500 rpm while oil pressure is still building and the
    // battery is still down from the starter; without this, every start fires
    // two critical alarms and teaches the driver to ignore red.
    uint8_t armDelayS;

    // Deadband, as a share of the limit. An alarm trips at the limit and only
    // clears once the value has moved back past it by this much, so a value
    // sitting on its threshold does not flicker.
    uint8_t hysteresisPercent;
};

AlarmSettings defaultAlarmSettings();

// An alarm that tripped at some point during this run, kept after the
// condition cleared.
//
// A half-second oil pressure dip in a corner is exactly the event worth
// knowing about and exactly the one a live-only display loses.
struct LatchedAlarm {
    AlarmId id = AlarmId::Count;
    const char* label = nullptr;
    const char* unit = nullptr;
    float value = 0.0f;
    uint16_t rpm = 0;
    AlarmSeverity severity = AlarmSeverity::None;
    uint8_t decimals = 1;
};

// Evaluates the snapshot against a fixed rule table.
//
// Lives beside the model rather than inside the screens: the same thresholds
// then apply on every page instead of being copied into four of them.
//
// An alarm never moves the page. It colours the cell that owns the value and
// names itself in the status bar, which is visible from every page anyway;
// taking the screen away from the driver bought nothing that the status bar
// was not already showing.
class AlarmEngine {
public:
    void evaluate(const EngineSnapshot& snapshot, uint32_t nowMs);

    AlarmSeverity severity(AlarmId id) const;
    AlarmSeverity worst() const { return worst_; }

    // Short label for the status bar, e.g. "OIL P 0.5 BAR". Empty when clear.
    const char* worstText() const { return worstText_; }

    bool engineRunning() const { return running_; }
    bool armed() const { return armed_; }

    // Whole seconds left of the arming delay, rounded up. Zero once armed.
    uint16_t armingRemainingS() const;

    uint8_t latchedCount() const { return latchedCount_; }
    const LatchedAlarm& latched(uint8_t index) const { return latched_[index]; }
    void clearLatched();

    AlarmSettings& settings() { return settings_; }
    const AlarmSettings& settings() const { return settings_; }

private:
    void latch(const LatchedAlarm& hit);

    AlarmSettings settings_ = defaultAlarmSettings();

    AlarmSeverity severity_[kAlarmCount] = {};
    LatchedAlarm latched_[kAlarmCount] = {};
    uint8_t latchedCount_ = 0;

    AlarmSeverity worst_ = AlarmSeverity::None;
    char worstText_[24] = {0};

    bool running_ = false;
    bool wasRunning_ = false;
    bool armed_ = false;
    uint32_t runningSinceMs_ = 0;
    uint32_t armingRemainingMs_ = 0;
};

}  // namespace ecu
