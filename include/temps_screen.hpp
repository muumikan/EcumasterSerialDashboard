#pragma once

#include "dash_page.hpp"
#include "ui_tile.hpp"

namespace ecu {

// Page 3 - TEMPS & PRESS. A parked check, so the cells are large.
class TempsScreen : public DashPage {
public:
    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;
    const char* name() const override { return "TEMPS & PRESS"; }

private:
    Tile clt_;
    Tile iat_;
    Tile ecuTemp_;
    Tile oil_;
    Tile fuel_;
    Tile deltaFpr_;
};

}  // namespace ecu
