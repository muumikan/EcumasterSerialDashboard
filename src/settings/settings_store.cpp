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
// shape. Either way the whole setup page reverts to defaults once - brightness
// and the shift points have to be set again after such an update.
constexpr uint32_t kMagic = 0x45435544;  // 'ECUD'
constexpr uint16_t kVersion = 4;

struct StoredSettings {
    uint32_t magic;
    uint16_t version;
    DashSettings settings;
};

}  // namespace

bool SettingsStore::load(DashSettings& out) {
    out = defaultDashSettings();

    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        return false;
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

    s.bootSweep = true;
    s.logging = true;

    // Off until it is asked for. The access point is a radio that comes up on
    // its own in a parked car, so it is opted into rather than out of.
    s.serviceAp = false;
    return s;
}

}  // namespace ecu
