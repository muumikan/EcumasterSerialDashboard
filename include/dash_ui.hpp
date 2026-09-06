#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "alarm_engine.hpp"
#include "dash_page.hpp"
#include "diagnostic_screen.hpp"
#include "engine_data_model.hpp"
#include "main_screen.hpp"
#include "temps_screen.hpp"
#include "tune_screen.hpp"

namespace ecu {

// Return to the driving page after this long without a touch, so the dash is
// never left showing diagnostics on the move.
constexpr uint32_t kIdleReturnMs = 30000;

// How long the shift lights sweep at power-up, the way a race dash does.
constexpr uint32_t kBootSweepMs = 900;

// Owns the always-on chrome (shift lights, status bar) and page navigation.
//
// The pages below it only render; deciding what is shown happens here. Nothing
// but the driver's own swipe changes the page - alarms announce themselves in
// the status bar, which is on screen whichever page is up.
class DashUi {
public:
    void begin(lv_obj_t* screen);
    void update(const EngineDataModel& model, uint32_t nowMs);

    void showPage(uint8_t index);
    void nextPage() { showPage(static_cast<uint8_t>((page_ + 1) % kPageCount)); }
    void previousPage() { showPage(static_cast<uint8_t>((page_ + kPageCount - 1) % kPageCount)); }

    void noteInteraction(uint32_t nowMs) { lastInteractionMs_ = nowMs; }

private:
    static constexpr uint8_t kPageCount = 4;
    static constexpr uint8_t kShiftSegments = 14;

    void buildChrome(lv_obj_t* screen);
    void setShiftSegments(uint8_t lit);
    bool runBootSweep(uint32_t nowMs);
    void updateShiftLights(uint16_t rpm);
    void updateStatusBar(const EngineDataModel& model, uint32_t nowMs);

    MainScreen drive_;
    TuneScreen tune_;
    TempsScreen temps_;
    DiagnosticScreen diagnostics_;
    DashPage* pages_[kPageCount] = {};

    AlarmEngine alarms_;
    RunPeaks peaks_;

    lv_obj_t* shift_[kShiftSegments] = {};
    lv_obj_t* dots_[kPageCount] = {};
    lv_obj_t* pageName_ = nullptr;
    lv_obj_t* alarmText_ = nullptr;
    lv_obj_t* linkText_ = nullptr;
    lv_obj_t* pageArea_ = nullptr;

    uint8_t page_ = 0;
    uint8_t litSegments_ = 0xFF;
    uint32_t bootSweepEndMs_ = 0;
    uint32_t lastRevision_ = UINT32_MAX;
    uint32_t lastInteractionMs_ = 0;
    LinkState lastLink_ = LinkState::Offline;
    AlarmSeverity lastWorst_ = AlarmSeverity::None;
};

}  // namespace ecu
