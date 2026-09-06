#include "ecu_data_provider.hpp"

#include "board_config.hpp"

namespace ecu {

EcuDataProvider::EcuDataProvider(HardwareSerial& uart, EngineDataModel& model)
    : uart_(uart), model_(model), adapter_(uart) {}

void EcuDataProvider::begin() {
    begin(board::kEcuBaud, board::kEcuRxPin, board::kEcuTxPin);
}

void EcuDataProvider::begin(uint32_t baud, int8_t rxPin, int8_t txPin) {
    uart_.begin(baud, SERIAL_8N1, rxPin, txPin);
}

void EcuDataProvider::loop(uint32_t nowMs) {
    if (adapter_.poll() == 0) {
        return;  // nothing arrived; leave the model's timestamps alone
    }

    model_.applySnapshot(adapter_.read(), nowMs);
}

}  // namespace ecu
