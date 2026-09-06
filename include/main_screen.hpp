#pragma once

#include "dash_page.hpp"
#include "ui_tile.hpp"

namespace ecu {

// Page 1 - DRIVE. On screen 95 % of the time: only what the driver needs.
class MainScreen : public DashPage {
public:
    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;
    const char* name() const override { return "DRIVE"; }

private:
    lv_obj_t* rpmValue_ = nullptr;
    lv_obj_t* rpmBar_ = nullptr;
    lv_obj_t* boostValue_ = nullptr;
    lv_obj_t* mapLabel_ = nullptr;
    lv_obj_t* boostFill_ = nullptr;
    lv_obj_t* boostPeakMark_ = nullptr;
    lv_obj_t* boostPeakLabel_ = nullptr;

    Tile clt_;
    Tile oil_;
    Tile lambda_;
    Tile battery_;

    float boostPeak_ = -9.0f;
};

}  // namespace ecu
