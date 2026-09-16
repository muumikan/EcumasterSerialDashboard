#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "alarm_engine.hpp"
#include "alarms_screen.hpp"
#include "boost_screen.hpp"
#include "dash_page.hpp"
#include "diagnostic_screen.hpp"
#include "engine_data_model.hpp"
#include "dash_settings.hpp"
#include "idle_screen.hpp"
#include "main_screen.hpp"
#include "rtc_clock.hpp"
#include "service_status.hpp"
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
    using ChangeCallback = void (*)(void* context);

    // The clock and the settings are borrowed, not owned: the log needs the
    // clock for its filenames and the settings for whether to run at all, and
    // both have to keep working when the display does not. `onChange` fires
    // after an edit so the owner can apply it outside the UI and save it.
    void begin(lv_obj_t* screen,
               const RtcClock& rtc,
               DashSettings& settings,
               ChangeCallback onChange,
               void* context);

    // `clockTicked` is true on the pass where the RTC produced a fresh reading.
    // The caller owns the tick, so the status bar and the alarm list's running
    // durations still advance when the ECU has gone quiet.
    void update(const EngineDataModel& model, uint32_t nowMs, bool clockTicked);

    // Offers the swipe to the page on screen first, then turns the page.
    // Returns true when something acted on it.
    bool handleGesture(lv_dir_t direction);

    void showPage(uint8_t index);
    void nextPage() { showPage(static_cast<uint8_t>((page_ + 1) % kPageCount)); }
    void previousPage() { showPage(static_cast<uint8_t>((page_ + kPageCount - 1) % kPageCount)); }

    void dismissSummary();

    // Applies the current settings to the UI and tells the owner.
    void settingsChanged();

    // State of the service access point, for the status bar. Pushed in rather
    // than read out: the UI does not otherwise know the radio exists.
    void setServiceState(const ServiceApStatus& status);

    // The alarm log, for the service page. Lives here because the alarms are
    // evaluated as the UI updates.
    const AlarmEngine& alarms() const { return alarms_; }

    // Re-reads the settings after someone else changed them - the service
    // page, which edits the same struct from outside the UI.
    void settingsReloaded() { applySettings(); }

private:
    static constexpr uint8_t kPageCount = 8;
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
    IdleScreen idle_;
    BoostScreen boost_;
    AlarmsScreen alarmList_;
    DiagnosticScreen diagnostics_;
    SetupScreen setup_;
    DashPage* pages_[kPageCount] = {};

    AlarmEngine alarms_;
    RunPeaks peaks_;
    const RtcClock* rtc_ = nullptr;

    DashSettings* settings_ = nullptr;
    ChangeCallback onChange_ = nullptr;
    void* context_ = nullptr;

    lv_obj_t* shift_[kShiftSegments] = {};
    lv_obj_t* dots_[kPageCount] = {};
    lv_obj_t* pageName_ = nullptr;
    lv_obj_t* alarmText_ = nullptr;
    lv_obj_t* latchBadge_ = nullptr;
    lv_obj_t* linkText_ = nullptr;
    lv_obj_t* clockText_ = nullptr;
    lv_obj_t* apText_ = nullptr;
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
    uint32_t lastEventTotal_ = UINT32_MAX;
    char clockShown_[6] = {0};
    bool apServing_ = false;
    uint8_t apClients_ = 0xFF;
    bool engineWasRunning_ = false;
};

}  // namespace ecu
