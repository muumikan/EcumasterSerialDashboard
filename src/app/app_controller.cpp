#include "app_controller.hpp"

#include "board_config.hpp"
#include "display.hpp"

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
        screen_.create();
    } else {
        Serial.println(F("display: LVGL buffer allocation failed, running headless"));
    }

    Serial.println();
    Serial.println(F("ECU dashboard - EMU Classic serial link (read-only)"));
}

void AppController::loop() {
    const uint32_t nowMs = millis();

    provider_.loop(nowMs);

    if (displayReady_) {
        screen_.update(model_, nowMs);
        display::loop();
    }

    report_.update(model_, nowMs);
}

}  // namespace ecu
