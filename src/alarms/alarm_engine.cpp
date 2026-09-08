#include "alarm_engine.hpp"

#include <math.h>
#include <stdio.h>

namespace ecu {
namespace {

// One rule per alarm. Only the *shape* lives here - which value is watched,
// which way it trips, how it reads. The numbers are settings, so the setup
// page can change them without being able to rewire the logic.
struct AlarmRule {
    AlarmId id;
    const char* label;
    const char* unit;
    int8_t direction;   // +1 trips high, -1 trips low
    uint8_t decimals;
    float (*value)(const EngineSnapshot&);
    bool (*applies)(const EngineSnapshot&);   // nullptr = always
};

float oilPressure(const EngineSnapshot& s) { return s.oilPressureBar; }
float coolant(const EngineSnapshot& s) { return static_cast<float>(s.cltC); }
float lambda(const EngineSnapshot& s) { return s.wboLambda; }
float battery(const EngineSnapshot& s) { return s.batteryV; }
float knock(const EngineSnapshot& s) { return s.knockLevelV; }
float injectorDuty(const EngineSnapshot& s) { return s.injDutyPct; }
float fuelPressure(const EngineSnapshot& s) { return s.fuelPressureBar; }
float intakeAir(const EngineSnapshot& s) { return static_cast<float>(s.iatC); }

// Cruise runs lean on purpose; only a lean mixture under boost is a fault.
bool underBoost(const EngineSnapshot& s) { return s.mapKpa > 140; }

// Order is priority order: when several alarms share a severity, the first one
// here is the one the status bar names. Oil pressure outranks intake air.
const AlarmRule kRules[kAlarmCount] = {
    { AlarmId::OilPressure,  "OIL P",  "bar", -1, 1, oilPressure,  nullptr },
    { AlarmId::Coolant,      "CLT",    "C",   +1, 0, coolant,      nullptr },
    { AlarmId::Lean,         "LEAN",   "",    +1, 2, lambda,       underBoost },
    { AlarmId::BatteryLow,   "BATT",   "V",   -1, 1, battery,      nullptr },
    { AlarmId::BatteryHigh,  "CHARGE", "V",   +1, 1, battery,      nullptr },
    { AlarmId::Knock,        "KNOCK",  "V",   +1, 1, knock,        nullptr },
    { AlarmId::InjectorDuty, "INJ DC", "%",   +1, 0, injectorDuty, nullptr },
    { AlarmId::FuelPressure, "FUEL P", "bar", -1, 1, fuelPressure, nullptr },
    { AlarmId::IntakeAir,    "IAT",    "C",   +1, 0, intakeAir,    nullptr },
};

// True while the value is on the wrong side of the limit. `active` widens the
// limit by the deadband, so an alarm that has tripped holds until the value has
// genuinely recovered rather than chattering on the threshold.
bool trips(float value, float limit, int8_t direction, bool active, float band) {
    const float margin = active ? fabsf(limit) * band : 0.0f;
    return direction > 0 ? (value > limit - margin) : (value < limit + margin);
}

}  // namespace

AlarmSettings defaultAlarmSettings() {
    AlarmSettings s = {};

    // Starting points, not measurements. Nothing here has been checked against
    // this engine - that is what the setup page is for.
    s.limits[static_cast<uint8_t>(AlarmId::OilPressure)]  = { 1.6f,  1.0f,  true };
    s.limits[static_cast<uint8_t>(AlarmId::Coolant)]      = { 100.0f, 108.0f, true };
    s.limits[static_cast<uint8_t>(AlarmId::Lean)]         = { 0.90f, 0.95f, true };
    s.limits[static_cast<uint8_t>(AlarmId::BatteryLow)]   = { 13.2f, 12.4f, true };
    s.limits[static_cast<uint8_t>(AlarmId::BatteryHigh)]  = { 15.0f, 15.5f, true };
    s.limits[static_cast<uint8_t>(AlarmId::Knock)]        = { 1.2f,  2.0f,  true };
    s.limits[static_cast<uint8_t>(AlarmId::InjectorDuty)] = { 85.0f, 92.0f, true };
    s.limits[static_cast<uint8_t>(AlarmId::FuelPressure)] = { 2.2f,  1.9f,  true };
    s.limits[static_cast<uint8_t>(AlarmId::IntakeAir)]    = { 65.0f, 75.0f, true };

    // Six seconds, not three: on this engine oil pressure had not finished
    // building by three, so the delay was still cutting it fine.
    s.armDelayS = 6;
    s.hysteresisPercent = 2;
    return s;
}

void AlarmEngine::evaluate(const EngineSnapshot& s,
                           LinkState link,
                           const DateTime& wall,
                           uint32_t nowMs) {
    events_.stampRunStart(wall);

    running_ = s.rpm > kEngineRunningRpm;

    if (running_ && !wasRunning_) {
        runningSinceMs_ = nowMs;
    }
    wasRunning_ = running_;

    const uint32_t delayMs = static_cast<uint32_t>(settings_.armDelayS) * 1000u;
    if (!running_) {
        armed_ = false;
        armingRemainingMs_ = delayMs;
    } else {
        const uint32_t up = nowMs - runningSinceMs_;
        armed_ = up >= delayMs;
        armingRemainingMs_ = armed_ ? 0 : delayMs - up;
    }

    worst_ = AlarmSeverity::None;
    worstText_[0] = '\0';

    const float band = settings_.hysteresisPercent / 100.0f;

    for (uint8_t i = 0; i < kAlarmCount; ++i) {
        const AlarmRule& rule = kRules[i];
        const uint8_t slot = static_cast<uint8_t>(rule.id);
        const AlarmLimits& limits = settings_.limits[slot];

        const AlarmSeverity previous = severity_[slot];
        AlarmSeverity now = AlarmSeverity::None;

        const bool considered =
            armed_ && limits.enabled && (rule.applies == nullptr || rule.applies(s));

        const float value = rule.value(s);
        if (considered) {
            if (trips(value, limits.crit, rule.direction, previous >= AlarmSeverity::Critical, band)) {
                now = AlarmSeverity::Critical;
            } else if (trips(value, limits.warn, rule.direction, previous >= AlarmSeverity::Warning, band)) {
                now = AlarmSeverity::Warning;
            }
        }
        severity_[slot] = now;

        if (now > previous) {
            LatchedAlarm hit;
            hit.id = rule.id;
            hit.label = rule.label;
            hit.unit = rule.unit;
            hit.value = value;
            hit.rpm = s.rpm;
            hit.severity = now;
            hit.decimals = rule.decimals;
            latch(hit);

            char reading[12];
            snprintf(reading, sizeof(reading), "%.*f%s%s",
                     rule.decimals, value,
                     rule.unit[0] != '\0' ? " " : "", rule.unit);
            events_.raise(static_cast<uint16_t>(kLimitKeyBase + slot), wall,
                          rule.label, reading, s.rpm, now, nowMs);
        } else if (now == AlarmSeverity::None && previous != AlarmSeverity::None) {
            events_.clear(static_cast<uint16_t>(kLimitKeyBase + slot), nowMs);
        }

        if (now > worst_) {
            worst_ = now;
            snprintf(worstText_, sizeof(worstText_), "%s %.*f %s",
                     rule.label, rule.decimals, value, rule.unit);
        }
    }

    recordCel(s, wall, nowMs);
    recordLink(link, s, wall, nowMs);
}

// The ECU's own check-engine word. Only the edges are interesting: a bit that
// stays set is one event, and a bit that goes away closes it rather than
// vanishing from the record the way the Diag page's grid lets it.
void AlarmEngine::recordCel(const EngineSnapshot& s, const DateTime& wall, uint32_t nowMs) {
    const uint16_t rising = static_cast<uint16_t>(s.celFlags & ~lastCel_);
    const uint16_t falling = static_cast<uint16_t>(lastCel_ & ~s.celFlags);
    lastCel_ = s.celFlags;

    if ((rising | falling) == 0) {
        return;
    }

    for (uint8_t bit = 0; bit < kCelBitCount; ++bit) {
        const uint16_t mask = static_cast<uint16_t>(1u << bit);
        const uint16_t key = static_cast<uint16_t>(kCelKeyBase + bit);
        if (rising & mask) {
            // The bit's name goes in the value column: "CEL / IAT" fits the
            // row, "CEL IAT" as one label does not.
            events_.raise(key, wall, "CEL", celBitName(bit), s.rpm,
                          AlarmSeverity::Warning, nowMs);
        } else if (falling & mask) {
            events_.clear(key, nowMs);
        }
    }
}

// Losing the link is the one fault the dashboard can see without the ECU's
// help, and the one a loose connector produces in clusters. Stale escalates
// into the same event rather than opening a second one.
void AlarmEngine::recordLink(LinkState link, const EngineSnapshot& s,
                             const DateTime& wall, uint32_t nowMs) {
    if (link == lastLink_) {
        return;
    }
    lastLink_ = link;

    if (link == LinkState::Online) {
        events_.clear(kLinkKey, nowMs);
    } else {
        events_.raise(kLinkKey, wall, "LINK", toString(link), s.rpm,
                      link == LinkState::Offline ? AlarmSeverity::Critical
                                                 : AlarmSeverity::Warning,
                      nowMs);
    }
}

void AlarmEngine::latch(const LatchedAlarm& hit) {
    for (uint8_t i = 0; i < latchedCount_; ++i) {
        if (latched_[i].id != hit.id) {
            continue;
        }
        // Already recorded: keep the worst moment, not the most recent one.
        if (hit.severity > latched_[i].severity) {
            latched_[i] = hit;
        }
        return;
    }

    if (latchedCount_ < kAlarmCount) {
        latched_[latchedCount_++] = hit;
    }
}

void AlarmEngine::clearLatched() {
    latchedCount_ = 0;
}

AlarmSeverity AlarmEngine::severity(AlarmId id) const {
    return severity_[static_cast<uint8_t>(id)];
}

uint16_t AlarmEngine::armingRemainingS() const {
    if (armed_ || !running_) {
        return 0;
    }
    return static_cast<uint16_t>((armingRemainingMs_ + 999) / 1000);
}

}  // namespace ecu
