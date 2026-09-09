#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "alarm_engine.hpp"
#include "engine_data_model.hpp"

namespace ecu {

// Peak hold for the run, cleared on power cycle. Recorded once by DashUi and
// read by the diagnostics page.
struct RunPeaks {
    uint16_t rpm = 0;
    uint16_t mapKpa = 0;
    int16_t cltC = 0;
    int8_t iatC = 0;
    float oilPressureMinBar = 0.0f;
    float oilPressureMaxBar = 0.0f;
    float fuelPressureMinBar = 0.0f;
    float fuelPressureMaxBar = 0.0f;
    float lambdaMin = 0.0f;
    float knockLevelV = 0.0f;
    float injDutyPct = 0.0f;
    bool seeded = false;

    void record(const EngineSnapshot& s);
};

// A dashboard page. Pages read the model and the alarm state; they never see
// a UART, a frame or a threshold of their own.
class DashPage {
public:
    virtual ~DashPage() = default;

    virtual void create(lv_obj_t* parent) = 0;
    virtual void update(const EngineDataModel& model,
                        const AlarmEngine& alarms,
                        const RunPeaks& peaks,
                        uint32_t nowMs) = 0;

    virtual const char* name() const = 0;

    // A page's chance to use a swipe before it turns the page. Return true to
    // say the gesture was consumed. Pages that fit on the screen - which is
    // all of them but the alarm list - want the default.
    //
    // Left and right are offered too, so a page could use them, but nothing
    // does: taking them away would leave the driver stuck on a page with no
    // obvious way off.
    virtual bool onSwipe(lv_dir_t direction) {
        (void)direction;
        return false;
    }

    lv_obj_t* root() const { return root_; }

protected:
    lv_obj_t* root_ = nullptr;
};

}  // namespace ecu
