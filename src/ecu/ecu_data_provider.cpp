#include "ecu_data_provider.hpp"

#include "board_config.hpp"

namespace ecu {

EcuDataProvider::EcuDataProvider(HardwareSerial& uart, EngineDataModel& model)
    : uart_(uart), model_(model), adapter_(uart) {}

void EcuDataProvider::begin() {
    begin(kEcuLinkBaud, board::kEcuRxPin, board::kEcuTxPin);
}

void EcuDataProvider::begin(uint32_t baud, int8_t rxPin, int8_t txPin) {
    // The Arduino default is 256 bytes, which is smaller than one 260-byte
    // EDL-1 frame and only ~22 ms of slack at 115200. Any redraw, card write
    // or NVS save that holds the loop longer than that drops bytes, and a
    // dropped byte splices two frames together - which reads as a burst of
    // impossible values across many fields at once. Must be set before begin().
    uart_.setRxBufferSize(2048);
    uart_.begin(baud, SERIAL_8N1, rxPin, txPin);
}

// The link is read-only by construction: this file and the adapter below it
// are the only code that touches the ECU port, and neither ever calls write(),
// print() or any other transmitting method on it.

void EcuDataProvider::loop(uint32_t nowMs) {
    if (adapter_.poll() == 0) {
        return;  // nothing arrived; leave the model's timestamps alone
    }

    model_.applySnapshot(adapter_.read(), nowMs);
}

}  // namespace ecu
