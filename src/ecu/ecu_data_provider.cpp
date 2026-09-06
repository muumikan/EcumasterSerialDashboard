#include "ecu_data_provider.hpp"

#include "board_config.hpp"

namespace ecu {

EcuDataProvider::EcuDataProvider(HardwareSerial& uart, EngineDataModel& model)
    : uart_(uart), model_(model), log_(), tap_(uart, log_), adapter_(tap_) {}

void EcuDataProvider::begin() {
    begin(board::kEcuBaud, board::kEcuRxPin, board::kEcuTxPin);
    log_.begin();
}

void EcuDataProvider::begin(uint32_t baud, int8_t rxPin, int8_t txPin) {
    uart_.begin(baud, SERIAL_8N1, rxPin, txPin);
}

// The link is read-only by construction: this file and the adapter below it
// are the only code that touches the ECU port, and neither ever calls write(),
// print() or any other transmitting method on it.

void EcuDataProvider::loop(uint32_t nowMs) {
    const uint32_t consumed = adapter_.poll();

    // Flush regardless, so a paused stream still commits what was buffered.
    log_.loop(nowMs);

    if (consumed == 0) {
        return;  // nothing arrived; leave the model's timestamps alone
    }

    model_.applySnapshot(adapter_.read(), nowMs);
}

}  // namespace ecu
