#pragma once

#include "dash_page.hpp"
#include "ui_tile.hpp"

namespace ecu {

// Page 3 - IDLE. What the idle loop is doing, for setting it up in the pit.
//
// The hero row is RPM against the idle target on one absolute 0..2000 rpm
// scale, rather than the deviation bar TUNE uses for mixture. A deviation only
// means anything while the loop is closed, and the moment the car is driven it
// would sit pegged for the whole session.
class IdleScreen : public DashPage {
public:
    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;
    const char* name() const override { return "IDLE"; }

private:
    Tile rpm_;
    lv_obj_t* targetLabel_ = nullptr;
    lv_obj_t* rpmFill_ = nullptr;
    lv_obj_t* targetMark_ = nullptr;

    Tile control_;
    Tile duty_;
    Tile pidCorrection_;
    Tile afr_;
    Tile ignition_;
    Tile angleCorrection_;
    Tile map_;
};

}  // namespace ecu
