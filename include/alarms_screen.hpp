#pragma once

#include <lvgl.h>

#include "dash_page.hpp"

namespace ecu {

// Everything that tripped this run, newest first, with the time it happened.
//
// The list the Diag page's "latched" block could never be: that one keeps a
// single slot per alarm type, so a coolant warning at 14:33 and another at
// 14:41 are one line. Here they are two, in the order they happened.
//
// There is no acknowledge button. This is a log the driver reads, not a queue
// an operator works through - in a car nobody is standing by to take
// responsibility for a row, and a button that only dims text is a button that
// takes attention away from the road.
class AlarmsScreen : public DashPage {
public:
    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;

    const char* name() const override { return "ALARMS"; }

private:
    static constexpr uint8_t kVisibleRows = 11;

    struct Row {
        lv_obj_t* root = nullptr;
        lv_obj_t* stripe = nullptr;
        lv_obj_t* time = nullptr;
        lv_obj_t* label = nullptr;
        lv_obj_t* value = nullptr;
        lv_obj_t* rpm = nullptr;
        lv_obj_t* duration = nullptr;
        lv_obj_t* state = nullptr;
    };

    void buildHeader();
    void buildRows();
    void buildFooter();
    void paint(const AlarmEngine& alarms, uint32_t nowMs);

    Row rows_[kVisibleRows];

    lv_obj_t* empty_ = nullptr;
    lv_obj_t* activeCount_ = nullptr;
    lv_obj_t* totalCount_ = nullptr;
    lv_obj_t* runTime_ = nullptr;
    lv_obj_t* since_ = nullptr;

    uint32_t lastRevision_ = UINT32_MAX;
    uint32_t lastPaintMs_ = 0;
};

}  // namespace ecu
