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
    Battery,
    Knock,
    InjectorDuty,
    FuelPressure,
    IntakeAir,
    Count,
};

// Below this the engine is not turning, so oil pressure, battery voltage and
// everything else that is only meaningful under load stays silent.
constexpr uint16_t kEngineRunningRpm = 500;

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
    void evaluate(const EngineSnapshot& snapshot);

    AlarmSeverity severity(AlarmId id) const;
    AlarmSeverity worst() const { return worst_; }

    // Short label for the status bar, e.g. "OIL P 0.5 BAR". Empty when clear.
    const char* worstText() const { return worstText_; }

    bool engineRunning() const { return running_; }

private:
    AlarmSeverity severity_[static_cast<uint8_t>(AlarmId::Count)] = {};
    AlarmSeverity worst_ = AlarmSeverity::None;
    char worstText_[24] = {0};
    bool running_ = false;
};

}  // namespace ecu
