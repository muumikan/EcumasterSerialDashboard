#pragma once

#include "dash_page.hpp"

namespace ecu {

// Page 4 - DIAG. Link health and this run's peaks.
class DiagnosticScreen : public DashPage {
public:
    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;
    const char* name() const override { return "DIAG"; }

private:
    // Link column.
    lv_obj_t* state_ = nullptr;
    lv_obj_t* age_ = nullptr;
    lv_obj_t* updates_ = nullptr;
    lv_obj_t* revision_ = nullptr;
    lv_obj_t* cel_ = nullptr;
    lv_obj_t* celBits_[16] = {};

    // The three most recent alarms that tripped during this run.
    lv_obj_t* latched_[3] = {};
    uint8_t lastLatchedCount_ = 0xFF;

    // Peaks column.
    lv_obj_t* peakValues_[11] = {};
};

}  // namespace ecu
