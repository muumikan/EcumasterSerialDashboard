#pragma once

#include <Arduino.h>

#include "dash_settings.hpp"
#include "dash_ui.hpp"
#include "ecu_data_provider.hpp"
#include "engine_data_model.hpp"
#include "rtc_clock.hpp"
#include "serial_report.hpp"
#include "service_ap.hpp"
#include "settings_store.hpp"

namespace ecu {

// Wires the layers together and owns their lifetimes, so main.cpp stays a
// two-line Arduino shim.
class AppController {
public:
    AppController();

    void begin();
    void loop();

    const EngineDataModel& model() const { return model_; }

    // Called from the UI and from the service page after either has edited the
    // settings. Applies them outside the UI and schedules the flash write.
    void settingsChanged();

private:
    // Prints what the console missed. See the call site in loop().
    void announce();

    // Opens the log once the clock can name it. See the call site in loop().
    void openLogWhenNamed();

    // Closes the finished drive's file when the engine stops, so it is a
    // complete file on the card rather than one still held open.
    void rotateLogWhenEngineStops(uint32_t nowMs);

    // Writes the settings once the edits have stopped. See kSettingsSaveDelayMs.
    void saveSettingsWhenSettled(uint32_t nowMs);

    HardwareSerial ecuUart_;
    EngineDataModel model_;
    EcuDataProvider provider_;
    SerialReport report_;

    // Owned here, not by the UI: the log needs the date for its filename, and
    // a dashboard whose display failed should still record the drive.
    RtcClock rtc_;

    // Owned here for the same reason as the clock: whether to log at all is a
    // setting, and the log has to work on a dashboard whose screen failed.
    DashSettings settings_;
    SettingsStore store_;

    ServiceAp serviceAp_;

    DashUi ui_;
    bool displayReady_ = false;
    bool consoleAnnounced_ = false;
    bool logOpened_ = false;
    bool engineWasRunning_ = false;
    bool settingsDirty_ = false;
    uint32_t saveDueMs_ = 0;
};

}  // namespace ecu
