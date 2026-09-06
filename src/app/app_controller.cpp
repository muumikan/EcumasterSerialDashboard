#include "app_controller.hpp"

#include "board_config.hpp"

namespace ecu {

AppController::AppController()
    : ecuUart_(1),  // UART1
      model_(),
      provider_(ecuUart_, model_),
      report_(Serial) {}

void AppController::begin() {
    Serial.begin(board::kDebugBaud);
    provider_.begin();

    Serial.println();
    Serial.println(F("ECU dashboard - EMU Classic serial link (read-only)"));
}

void AppController::loop() {
    const uint32_t nowMs = millis();

    provider_.loop(nowMs);
    report_.update(model_, nowMs);
}

}  // namespace ecu
