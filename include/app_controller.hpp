#pragma once

#include <Arduino.h>

#include "dash_ui.hpp"
#include "ecu_data_provider.hpp"
#include "engine_data_model.hpp"
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
    HardwareSerial ecuUart_;
    EngineDataModel model_;
    EcuDataProvider provider_;
    SerialReport report_;
    DashUi ui_;
    bool displayReady_ = false;
};

}  // namespace ecu
