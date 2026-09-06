#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "engine_data_model.hpp"

namespace ecu {

// The driver-facing dashboard. Reads the model and nothing else - it has no
// idea a serial port exists.
class MainScreen {
public:
    void create();

    // Cheap to call every loop: redraws only when the model actually changed.
    void update(const EngineDataModel& model, uint32_t nowMs);

private:
    enum Tile : uint8_t {
        kMap,
        kTps,
        kClt,
        kIat,
        kBattery,
        kLambda,
        kOilPressure,
        kOilTemp,
        kTileCount,
    };

    lv_obj_t* linkLabel_ = nullptr;
    lv_obj_t* rpmLabel_ = nullptr;
    lv_obj_t* tileValues_[kTileCount] = {};

    uint32_t lastRevision_ = UINT32_MAX;
    LinkState lastState_ = LinkState::Offline;
};

}  // namespace ecu
