#include "app_controller.hpp"

#include <lvgl.h>

#include "alarm_engine.hpp"
#include "board_config.hpp"
#include "date_time.hpp"
#include "display.hpp"
#include "ecu_link.hpp"
#include "rtc_clock.hpp"

namespace ecu {
namespace {

void settingsChangedCb(void* context) {
    static_cast<AppController*>(context)->settingsChanged();
}

}  // namespace

AppController::AppController()
    : ecuUart_(1),  // UART1
      model_(),
      provider_(ecuUart_, model_),
      report_(Serial) {}

void AppController::begin() {
    Serial.begin(board::kDebugBaud);

    provider_.begin();

    if (!store_.load(settings_)) {
        Serial.println(F("settings: no stored record, using defaults"));
    }

    // Before the display: the log is named after the date, and a dashboard
    // whose screen failed should still record the drive.
    rtc_.begin();
    openLogWhenNamed();
    serviceAp_.begin(settings_);

    displayReady_ = display::begin();
    if (displayReady_) {
        ui_.begin(lv_scr_act(), rtc_, settings_, settingsChangedCb, this);
    } else {
        Serial.println(F("display: LVGL buffer allocation failed, running headless"));
    }

    ServiceContext service;
    service.log = &provider_.log();
    service.model = &model_;
    service.rtc = &rtc_;
    service.ap = &serviceAp_;
    service.alarms = displayReady_ ? &ui_.alarms() : nullptr;
    service.settings = &settings_;
    service.onSettingsChanged = settingsChangedCb;
    service.pump = pumpDisplay;
    service.context = this;
    servicePage_.begin(service);

    Serial.println();
    Serial.println(F("ECU dashboard - EMU Classic serial link (read-only)"));
}

// Sending a few megabytes off the card is seconds of work. Without this the
// screen would be frozen for all of it, which on a dashboard reads as a crash.
void AppController::pumpDisplay(void* context) {
    AppController* self = static_cast<AppController*>(context);
    if (self->displayReady_) {
        display::loop();
    }
}

// Everything this firmware has to say about its own state is printed inside
// the first second after reset - long before a monitor opened after the upload
// has attached to the port, so the console shows an empty screen and no way to
// tell whether the board is alive. Waiting for the host at boot would be worse:
// in the car nobody is listening and the dashboard would just come up later.
//
// So say it again once loop() starts. That is not enough on its own: `Serial`
// reads as connected on this USB-CDC port well before the host has actually
// re-opened it after a reset, so on roughly half the resets this print still
// goes nowhere. Hence the second trigger in loop() - press any key and the
// board says where it stands, whenever you ask.
void AppController::announce() {
    Serial.println();
    Serial.println(F("---------------- DASH ----------------"));

    Serial.print(F("build     : "));
    Serial.print(F(__DATE__));
    Serial.print(' ');
    Serial.println(F(__TIME__));

    Serial.print(F("ECU link  : "));
    Serial.print(kEcuLinkName);
    Serial.print(F(" @ "));
    Serial.print(kEcuLinkBaud);
    Serial.print(F(" baud, "));
    Serial.println(toString(model_.linkState(millis())));

    Serial.print(F("clock     : "));
    if (rtc_.present()) {
        char stamp[9];
        formatHms(rtc_.now(), stamp, sizeof(stamp));
        Serial.println(stamp);
    } else {
        Serial.println(F("not available"));
    }

    Serial.print(F("log       : "));
    if (provider_.log().ready()) {
        Serial.print(provider_.log().fileName());
        Serial.print(F("  frames="));
        Serial.print(provider_.log().framesWritten());
        Serial.print(F("  bytes="));
        Serial.print(provider_.log().bytesWritten());
        if (provider_.log().framesDropped() > 0) {
            Serial.print(F("  dropped="));
            Serial.print(provider_.log().framesDropped());
        }
        Serial.println();
    } else {
        Serial.print(F("off - "));
        Serial.println(provider_.log().failure());
    }

    // Says the feature is in this build even when the radio is down, and names
    // the one condition holding it down. Five of them can, and none is visible
    // from outside the car.
    Serial.print(F("service   : "));
    if (serviceAp_.serving()) {
        Serial.print(serviceAp_.ssid());
        Serial.print(F(" at http://"));
        Serial.print(serviceAp_.ipAddress());
        Serial.print(F("  clients="));
        Serial.println(serviceAp_.clients());
    } else {
        Serial.print(F("down - "));
        Serial.print(serviceAp_.reason());
        const uint32_t left = serviceAp_.armingSecondsLeft(millis());
        if (left > 0) {
            Serial.print(F(" ("));
            Serial.print(left);
            Serial.print(F(" s to go)"));
        }
        Serial.println();
    }

    Serial.print(F("display   : "));
    if (displayReady_) {
        Serial.println(F("up"));
    } else {
        Serial.println(F("failed, running headless"));
    }
    Serial.println();
}

// The log file is named after the date and nothing else in the format carries
// it, so there is no log until the clock can name one. Retried every pass
// because the RTC is on the touch I2C bus and may answer late.
void AppController::openLogWhenNamed() {
    if (logOpened_) {
        return;
    }
    if (!settings_.logging) {
        // Said once, not every pass: the reason has to survive to the console
        // and to the service page, but it is not news on the second loop.
        logOpened_ = true;
        provider_.log().disable("turned off in setup");
        return;
    }
    if (rtc_.now().valid) {
        logOpened_ = true;
        provider_.log().begin(rtc_.now());
        return;
    }
    if (!rtc_.present()) {
        // No clock at all. Naming the log after the build gives a wrong date
        // on a readable file, which beats not recording the drive. The
        // collision suffix in EmuLog keeps a second run from erasing the
        // first, since every run would otherwise pick the same name.
        logOpened_ = true;
        Serial.println(F("emulog: no RTC, naming the log after the build"));
        provider_.log().begin(buildTime());
    }
}

// The engine stopping is the natural end of a drive, and the point at which
// the file for it should be complete rather than still open. Rotating here
// means the log can be fetched over the service access point without waiting
// for the next power cycle.
void AppController::rotateLogWhenEngineStops(uint32_t nowMs) {
    const bool running = model_.snapshot().rpm >= kEngineRunningRpm &&
                         model_.linkState(nowMs) != LinkState::Offline;
    if (running == engineWasRunning_) {
        return;
    }
    engineWasRunning_ = running;

    // Only on the falling edge, and only when the file has something in it.
    // Otherwise every key-on that never fires the engine leaves an empty file.
    if (running || !provider_.log().ready() || provider_.log().framesWritten() == 0) {
        return;
    }
    provider_.log().rotate(rtc_.now());
}

void AppController::settingsChanged() {
    // The service page edits the same struct the panel does, so the UI has to
    // be told either way. Applying twice after a panel edit costs nothing.
    if (displayReady_) {
        ui_.settingsReloaded();
    }

    settingsDirty_ = true;
    saveDueMs_ = millis() + kSettingsSaveDelayMs;

    // Logging can be turned off and on again without a reboot. Turning it off
    // closes the file properly rather than abandoning it, which matters: the
    // last few seconds live in a RAM buffer until something flushes them.
    if (settings_.logging && !provider_.log().ready()) {
        provider_.log().enable();
        logOpened_ = false;
    } else if (!settings_.logging && provider_.log().ready()) {
        provider_.log().end();
        provider_.log().disable("turned off in setup");
    }
}

// Edits are written once the driver stops adjusting, not per button press.
void AppController::saveSettingsWhenSettled(uint32_t nowMs) {
    if (!settingsDirty_ || static_cast<int32_t>(nowMs - saveDueMs_) < 0) {
        return;
    }
    settingsDirty_ = false;
    store_.save(settings_);
    Serial.println(F("settings: saved"));
}

void AppController::loop() {
    const uint32_t nowMs = millis();

    if (!consoleAnnounced_ && Serial) {
        consoleAnnounced_ = true;
        announce();
    }

    // Anything typed at the console reprints the state. The link is read-only
    // towards the ECU; this reads the debug port, which is a different UART.
    if (Serial.available() > 0) {
        while (Serial.available() > 0) {
            Serial.read();
        }
        announce();
    }

    const bool clockTicked = rtc_.loop(nowMs);
    openLogWhenNamed();

    provider_.loop(nowMs);
    rotateLogWhenEngineStops(nowMs);
    saveSettingsWhenSettled(nowMs);
    serviceAp_.loop(model_, nowMs);
    servicePage_.loop(serviceAp_.serving());

    if (displayReady_) {
        ui_.setServiceState(serviceAp_.status());
        ui_.update(model_, nowMs, clockTicked);
        display::loop();
    }

    report_.update(model_, nowMs);
}

}  // namespace ecu
