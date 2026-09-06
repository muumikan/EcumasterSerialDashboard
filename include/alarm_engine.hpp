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
class AlarmEngine {
public:
    void evaluate(const EngineSnapshot& snapshot);

    AlarmSeverity severity(AlarmId id) const;
    AlarmSeverity worst() const { return worst_; }

    // Short label for the status bar, e.g. "OIL P 0.5 BAR". Empty when clear.
    const char* worstText() const { return worstText_; }

    bool engineRunning() const { return running_; }

    // A critical alarm asks once, on its rising edge, for the page that shows
    // it. Consuming the request means the dash never fights the driver: swipe
    // away and it stays away until the condition clears and returns.
    bool takeCriticalPageRequest(uint8_t& pageOut);

private:
    AlarmSeverity severity_[static_cast<uint8_t>(AlarmId::Count)] = {};
    AlarmSeverity worst_ = AlarmSeverity::None;
    char worstText_[24] = {0};
    bool running_ = false;
    bool pageRequested_ = false;
    uint8_t requestedPage_ = 0;
};

}  // namespace ecu
