#pragma once

#include "dash_page.hpp"
#include "ui_tile.hpp"

namespace ecu {

// Page 2 - TUNE. Mixture, timing and knock side by side for dyno work.
class TuneScreen : public DashPage {
public:
    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;
    const char* name() const override { return "TUNE"; }

private:
    Tile lambda_;
    lv_obj_t* targetLabel_ = nullptr;
    lv_obj_t* deviationFill_ = nullptr;

    Tile knock_;
    Tile ignition_;
    Tile pulseWidth_;
    Tile dutyCycle_;
    Tile map_;
    Tile tps_;
    Tile rpm_;
};

}  // namespace ecu
