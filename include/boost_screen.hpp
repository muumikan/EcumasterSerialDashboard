#pragma once

#include "dash_page.hpp"
#include "ui_tile.hpp"

namespace ecu {

// Page 5 - BOOST. The boost loop's own working state, laid out like IDLE.
//
// Pressure is shown in absolute kPa here, not the gauge bar DRIVE uses. That
// is the unit the ECU states boostTarget in, and reading target and actual off
// one scale is the whole point of the page.
class BoostScreen : public DashPage {
public:
    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;
    const char* name() const override { return "BOOST"; }

private:
    Tile map_;
    lv_obj_t* targetLabel_ = nullptr;
    lv_obj_t* mapFill_ = nullptr;
    lv_obj_t* targetMark_ = nullptr;

    Tile tableSet_;
    Tile duty_;
    Tile pidCorrection_;
    Tile errorCorrection_;
    Tile rpm_;
    Tile tps_;
    Tile afr_;
};

}  // namespace ecu
