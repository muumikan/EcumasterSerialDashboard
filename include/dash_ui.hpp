#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "alarm_engine.hpp"
#include "dash_page.hpp"
#include "diagnostic_screen.hpp"
#include "engine_data_model.hpp"
#include "dash_settings.hpp"
#include "main_screen.hpp"
#include "rtc_clock.hpp"
#include "settings_store.hpp"
#include "setup_screen.hpp"
#include "temps_screen.hpp"
#include "tune_screen.hpp"

namespace ecu {

// Edits are collected and written to flash this long after the last one, so a
// held + button costs one NVS write rather than twenty.
constexpr uint32_t kSettingsSaveDelayMs = 4000;

// How long the shift lights sweep at power-up, the way a race dash does.
// Long enough to actually watch: at 900 ms it was over before you looked up.
constexpr uint32_t kBootSweepMs = 2600;

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

    void dismissSummary();

    // Applies the current settings everywhere and schedules a save.
    void settingsChanged();

private:
    static constexpr uint8_t kPageCount = 5;
    static constexpr uint8_t kShiftSegments = 14;

    void buildChrome(lv_obj_t* screen);
    void applySettings();
    void buildSummary(lv_obj_t* screen);
    void updateSummary(LinkState link);
    void setShiftSegments(uint8_t lit);
    bool runBootSweep(uint32_t nowMs);
    void updateShiftLights(uint16_t rpm);
    void updateStatusBar(const EngineDataModel& model, uint32_t nowMs);

    MainScreen drive_;
    TuneScreen tune_;
    TempsScreen temps_;
    DiagnosticScreen diagnostics_;
    SetupScreen setup_;
    DashPage* pages_[kPageCount] = {};

    AlarmEngine alarms_;
    RunPeaks peaks_;
    RtcClock rtc_;

    DashSettings settings_;
    SettingsStore store_;
    bool settingsDirty_ = false;
    uint32_t saveDueMs_ = 0;

    lv_obj_t* shift_[kShiftSegments] = {};
    lv_obj_t* dots_[kPageCount] = {};
    lv_obj_t* pageName_ = nullptr;
    lv_obj_t* alarmText_ = nullptr;
    lv_obj_t* latchBadge_ = nullptr;
    lv_obj_t* linkText_ = nullptr;
    lv_obj_t* clockText_ = nullptr;
    lv_obj_t* pageArea_ = nullptr;

    lv_obj_t* summary_ = nullptr;
    lv_obj_t* summaryHead_ = nullptr;
    lv_obj_t* summaryBody_ = nullptr;

    uint8_t page_ = 0;
    uint8_t litSegments_ = 0xFF;
    uint32_t bootSweepEndMs_ = 0;
    uint32_t lastRevision_ = UINT32_MAX;
    LinkState lastLink_ = LinkState::Offline;
    AlarmSeverity lastWorst_ = AlarmSeverity::None;
    uint8_t lastLatched_ = 0xFF;
    char clockShown_[6] = {0};
    bool engineWasRunning_ = false;
};

}  // namespace ecu
