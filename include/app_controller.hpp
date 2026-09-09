#pragma once

#include <Arduino.h>

#include "dash_ui.hpp"
#include "ecu_data_provider.hpp"
#include "engine_data_model.hpp"
#include "rtc_clock.hpp"
#include "serial_report.hpp"

namespace ecu {

// Wires the layers together and owns their lifetimes, so main.cpp stays a
// two-line Arduino shim.
class AppController {
public:
    AppController();

    void begin();
    void loop();

    const EngineDataModel& model() const { return model_; }

private:
    // Prints what the console missed. See the call site in loop().
    void announce();

    // Opens the log once the clock can name it. See the call site in loop().
    void openLogWhenNamed();

    HardwareSerial ecuUart_;
    EngineDataModel model_;
    EcuDataProvider provider_;
    SerialReport report_;

    // Owned here, not by the UI: the log needs the date for its filename, and
    // a dashboard whose display failed should still record the drive.
    RtcClock rtc_;

    DashUi ui_;
    bool displayReady_ = false;
    bool consoleAnnounced_ = false;
    bool logOpened_ = false;
};

}  // namespace ecu
