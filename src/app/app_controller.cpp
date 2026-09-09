#include "app_controller.hpp"

#include <lvgl.h>

#include "board_config.hpp"
#include "date_time.hpp"
#include "display.hpp"
#include "ecu_link.hpp"

namespace ecu {

AppController::AppController()
    : ecuUart_(1),  // UART1
      model_(),
      provider_(ecuUart_, model_),
      report_(Serial) {}

void AppController::begin() {
    Serial.begin(board::kDebugBaud);

    provider_.begin();

    displayReady_ = display::begin();
    if (displayReady_) {
        ui_.begin(lv_scr_act());
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
// So say it again, once, at the moment the port is actually opened.
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
    if (displayReady_ && ui_.clock().present()) {
        char stamp[9];
        formatHms(ui_.clock().now(), stamp, sizeof(stamp));
        Serial.println(stamp);
    } else {
        Serial.println(F("not available"));
    }

    Serial.print(F("log       : "));
    if (provider_.log().ready()) {
        Serial.print(provider_.log().fileName());
        Serial.print(F("  frames="));
        Serial.println(provider_.log().framesWritten());
    } else {
        Serial.println(F("off, no card"));
    }

    Serial.print(F("display   : "));
    if (displayReady_) {
        Serial.println(F("up"));
    } else {
        Serial.println(F("failed, running headless"));
    }
    Serial.println();
}

void AppController::loop() {
    const uint32_t nowMs = millis();

    if (!consoleAnnounced_ && Serial) {
        consoleAnnounced_ = true;
        announce();
    }

    provider_.loop(nowMs);

    if (displayReady_) {
        ui_.update(model_, nowMs);
        display::loop();
    }

    report_.update(model_, nowMs);
}

}  // namespace ecu
