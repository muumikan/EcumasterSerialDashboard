#include "settings_store.hpp"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

namespace ecu {
namespace {

constexpr char kNamespace[] = "dash";
constexpr char kKey[] = "cfg";

// Bumped whenever DashSettings changes shape. A stored record with a different
// version is ignored rather than reinterpreted, so a firmware update can never
// come up with a threshold read out of the wrong bytes.
//
// Version 3 did not change the shape: it forced the stored record to be
// dropped so the new defaults for the battery and fuel-pressure alarms took
// effect. Version 4 adds the service access point flag, which does change the
// shape. Either way the whole setup page reverted to defaults once.
//
// Version 5 adds the overboost alarm and the mixture unit, and changes what
// the knock limits mean. It is the first that migrates rather than resets: a
// version 4 record is read through its own layout below, so brightness, shift
// points and every other limit survive the update.
constexpr uint32_t kMagic = 0x45435544;  // 'ECUD'
constexpr uint16_t kVersion = 5;

struct StoredSettings {
    uint32_t magic;
    uint16_t version;
    DashSettings settings;
};

// ---- version 4, frozen ------------------------------------------------------
// The shape exactly as version 4 stored it. Never edit this: it describes
// bytes already sitting in flash on the car.
constexpr uint16_t kVersion4 = 4;
constexpr uint8_t kAlarmCountV4 = 9;   // OilPressure .. IntakeAir

struct AlarmSettingsV4 {
    AlarmLimits limits[kAlarmCountV4];
    uint8_t armDelayS;
    uint8_t hysteresisPercent;
};

struct DashSettingsV4 {
    AlarmSettingsV4 alarms;
    uint16_t shiftFirstRpm;
    uint16_t shiftRedRpm;
    uint16_t shiftAllRpm;
    uint8_t brightnessPct;
    uint8_t nightBrightnessPct;
    bool nightMode;
    bool bootSweep;
    bool logging;
    bool serviceAp;
};

struct StoredSettingsV4 {
    uint32_t magic;
    uint16_t version;
    DashSettingsV4 settings;
};

// Checked against the version 4 DashSettings on 2026-09-24, field by field.
static_assert(sizeof(DashSettingsV4) == 124, "the version 4 layout must not move");
static_assert(sizeof(StoredSettingsV4) != sizeof(StoredSettings),
              "load() tells the versions apart by length");

// Everything version 4 knew carries over; what it did not know takes the new
// defaults, which `out` already holds.
void migrateFrom4(const DashSettingsV4& in, DashSettings& out) {
    for (uint8_t i = 0; i < kAlarmCountV4; ++i) {
        out.alarms.limits[i] = in.alarms.limits[i];
    }
    out.alarms.armDelayS = in.alarms.armDelayS;
    out.alarms.hysteresisPercent = in.alarms.hysteresisPercent;

    // Knock keeps only its on/off switch. Its limits were volts of knockLevel
    // and are now a ratio of knockValue to engine noise, so an old number
    // would mean something else entirely.
    const uint8_t knock = static_cast<uint8_t>(AlarmId::Knock);
    const bool knockOn = out.alarms.limits[knock].enabled;
    out.alarms.limits[knock] = defaultAlarmSettings().limits[knock];
    out.alarms.limits[knock].enabled = knockOn;

    out.shiftFirstRpm = in.shiftFirstRpm;
    out.shiftRedRpm = in.shiftRedRpm;
    out.shiftAllRpm = in.shiftAllRpm;
    out.brightnessPct = in.brightnessPct;
    out.nightBrightnessPct = in.nightBrightnessPct;
    out.nightMode = in.nightMode;
    out.bootSweep = in.bootSweep;
    out.logging = in.logging;
    out.serviceAp = in.serviceAp;
}

}  // namespace

bool SettingsStore::load(DashSettings& out) {
    out = defaultDashSettings();

    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        return false;
    }

    const size_t length = prefs.getBytesLength(kKey);

    if (length == sizeof(StoredSettingsV4)) {
        StoredSettingsV4 old = {};
        prefs.getBytes(kKey, &old, sizeof(old));
        prefs.end();
        if (old.magic != kMagic || old.version != kVersion4) {
            return false;
        }
        migrateFrom4(old.settings, out);
        Serial.println(F("settings: migrated from version 4"));
        save(out);   // once, so the next boot reads version 5 directly
        return true;
    }

    StoredSettings stored = {};
    const size_t read = prefs.getBytes(kKey, &stored, sizeof(stored));
    prefs.end();

    if (read != sizeof(stored) || stored.magic != kMagic || stored.version != kVersion) {
        return false;
    }

    out = stored.settings;
    return true;
}

void SettingsStore::save(const DashSettings& in) {
    StoredSettings stored = {};
    stored.magic = kMagic;
    stored.version = kVersion;
    stored.settings = in;

    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        Serial.println(F("settings: could not open NVS for writing"));
        return;
    }
    prefs.putBytes(kKey, &stored, sizeof(stored));
    prefs.end();
}

DashSettings defaultDashSettings() {
    DashSettings s = {};
    s.alarms = defaultAlarmSettings();

    // Set from watching the car rather than guessed at, unlike the first pass.
    s.shiftFirstRpm = 1000;
    s.shiftRedRpm = 6000;
    s.shiftAllRpm = 7200;

    s.brightnessPct = 90;
    s.nightBrightnessPct = 35;
    s.nightMode = false;
    s.mixtureAfr = false;

    s.bootSweep = true;
    s.logging = true;

    // Off until it is asked for. The access point is a radio that comes up on
    // its own in a parked car, so it is opted into rather than out of.
    s.serviceAp = false;
    return s;
}

}  // namespace ecu
