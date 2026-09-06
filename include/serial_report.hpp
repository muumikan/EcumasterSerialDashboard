#pragma once

#include <Arduino.h>

#include "engine_data_model.hpp"

namespace ecu {

// Text diagnostics on the debug console. Reads the model exactly the way the
// LVGL screens will, so it doubles as a check that the model is usable
// without any protocol knowledge.
class SerialReport {
public:
    explicit SerialReport(Print& out);

    // Prints at most once every intervalMs, and only when something changed
    // or the link state moved.
    void update(const EngineDataModel& model, uint32_t nowMs, uint32_t intervalMs = 1000);

private:
    void print(const EngineDataModel& model, uint32_t nowMs);

    Print& out_;
    uint32_t lastPrintMs_ = 0;
    uint32_t lastRevision_ = UINT32_MAX;
    LinkState lastState_ = LinkState::Offline;
};

}  // namespace ecu
