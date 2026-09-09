#include "app_controller.hpp"

#include <lvgl.h>

#include "board_config.hpp"
#include "date_time.hpp"
#include "display.hpp"
#include "ecu_link.hpp"
#include "rtc_clock.hpp"

namespace ecu {

AppController::AppController()
    : ecuUart_(1),  // UART1
      model_(),
      provider_(ecuUart_, model_),
      report_(Serial) {}

void AppController::begin() {
    Serial.begin(board::kDebugBaud);

    provider_.begin();

    // Before the display: the log is named after the date, and a dashboard
    // whose screen failed should still record the drive.
    rtc_.begin();
    openLogWhenNamed();

    displayReady_ = display::begin();
    if (displayReady_) {
        ui_.begin(lv_scr_act(), rtc_);
    } else {
        Serial.println(F("display: LVGL buffer allocation failed, running headless"));
    }

    Serial.println();
    Serial.println(F("ECU dashboard - EMU Classic serial link (read-only)"));
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

    if (displayReady_) {
        ui_.update(model_, nowMs, clockTicked);
        display::loop();
    }

    report_.update(model_, nowMs);
}

}  // namespace ecu
