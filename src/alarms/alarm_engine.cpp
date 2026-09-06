#include "alarm_engine.hpp"

#include <stdio.h>

namespace ecu {
namespace {

struct AlarmRule {
    AlarmId id;
    AlarmSeverity (*evaluate)(const EngineSnapshot&);
    void (*format)(const EngineSnapshot&, char*, size_t);
};

AlarmSeverity oilPressure(const EngineSnapshot& s) {
    if (s.oilPressureBar < 1.0f) return AlarmSeverity::Critical;
    if (s.oilPressureBar < 1.6f) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void oilPressureText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "OIL P %.1f BAR", s.oilPressureBar);
}

AlarmSeverity coolant(const EngineSnapshot& s) {
    if (s.cltC > 108) return AlarmSeverity::Critical;
    if (s.cltC > 100) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void coolantText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "CLT %d C", static_cast<int>(s.cltC));
}

// Lean only matters under boost; cruise runs lean on purpose.
AlarmSeverity lean(const EngineSnapshot& s) {
    if (s.mapKpa <= 140) return AlarmSeverity::None;
    if (s.wboLambda > 0.95f) return AlarmSeverity::Critical;
    if (s.wboLambda > 0.90f) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void leanText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "LEAN %.2f", s.wboLambda);
}

AlarmSeverity battery(const EngineSnapshot& s) {
    if (s.batteryV < 12.4f) return AlarmSeverity::Critical;
    if (s.batteryV < 13.2f || s.batteryV > 15.0f) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void batteryText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "BATT %.1f V", s.batteryV);
}

AlarmSeverity knock(const EngineSnapshot& s) {
    if (s.knockLevelV > 2.0f) return AlarmSeverity::Critical;
    if (s.knockLevelV > 1.2f) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void knockText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "KNOCK %.1f V", s.knockLevelV);
}

AlarmSeverity injectorDuty(const EngineSnapshot& s) {
    if (s.injDutyPct > 92.0f) return AlarmSeverity::Critical;
    if (s.injDutyPct > 85.0f) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void injectorDutyText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "INJ DC %d %%", static_cast<int>(s.injDutyPct));
}

AlarmSeverity fuelPressure(const EngineSnapshot& s) {
    if (s.fuelPressureBar < 2.6f) return AlarmSeverity::Critical;
    if (s.fuelPressureBar < 3.2f) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void fuelPressureText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "FUEL P %.1f BAR", s.fuelPressureBar);
}

AlarmSeverity intakeAir(const EngineSnapshot& s) {
    if (s.iatC > 75) return AlarmSeverity::Critical;
    if (s.iatC > 65) return AlarmSeverity::Warning;
    return AlarmSeverity::None;
}
void intakeAirText(const EngineSnapshot& s, char* out, size_t n) {
    snprintf(out, n, "IAT %d C", static_cast<int>(s.iatC));
}

// Order is priority order: when several alarms share a severity, the first
// one here is the one the status bar names.
const AlarmRule kRules[] = {
    { AlarmId::OilPressure,  oilPressure,  oilPressureText },
    { AlarmId::Coolant,      coolant,      coolantText },
    { AlarmId::Lean,         lean,         leanText },
    { AlarmId::Battery,      battery,      batteryText },
    { AlarmId::Knock,        knock,        knockText },
    { AlarmId::InjectorDuty, injectorDuty, injectorDutyText },
    { AlarmId::FuelPressure, fuelPressure, fuelPressureText },
    { AlarmId::IntakeAir,    intakeAir,    intakeAirText },
};
constexpr size_t kRuleCount = sizeof(kRules) / sizeof(kRules[0]);

}  // namespace

void AlarmEngine::evaluate(const EngineSnapshot& s) {
    running_ = s.rpm > kEngineRunningRpm;

    worst_ = AlarmSeverity::None;
    worstText_[0] = '\0';

    for (size_t i = 0; i < kRuleCount; ++i) {
        const AlarmRule& rule = kRules[i];
        const uint8_t slot = static_cast<uint8_t>(rule.id);

        const AlarmSeverity now = running_ ? rule.evaluate(s) : AlarmSeverity::None;
        severity_[slot] = now;

        if (now > worst_) {
            worst_ = now;
            rule.format(s, worstText_, sizeof(worstText_));
        }
    }
}

AlarmSeverity AlarmEngine::severity(AlarmId id) const {
    return severity_[static_cast<uint8_t>(id)];
}

}  // namespace ecu
