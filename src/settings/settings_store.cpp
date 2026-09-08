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
constexpr uint32_t kMagic = 0x45435544;  // 'ECUD'
constexpr uint16_t kVersion = 1;

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

void SettingsStore::clear() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return;
    }
    prefs.remove(kKey);
    prefs.end();
}

DashSettings defaultDashSettings() {
    DashSettings s = {};
    s.alarms = defaultAlarmSettings();

    // Guesses for a 1G-GTE, and the reason the setup page exists.
    s.shiftFirstRpm = 3500;
    s.shiftRedRpm = 6800;
    s.shiftAllRpm = 7200;

    s.brightnessPct = 90;
    s.nightBrightnessPct = 35;
    s.nightMode = false;

    s.idleReturnS = 30;
    s.bootSweep = true;
    s.logging = true;
    return s;
}

}  // namespace ecu
