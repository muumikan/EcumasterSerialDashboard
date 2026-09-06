#pragma once

#include <Arduino.h>

#include "emu_log.hpp"
#include "emu_serial_adapter.hpp"
#include "engine_data_model.hpp"

namespace ecu {

// Owns the UART and pumps decoded data into the model.
//
// This is the seam between hardware and application state: the model above it
// never touches a Stream, and the adapter below it never touches timing.
class EcuDataProvider {
public:
    EcuDataProvider(HardwareSerial& uart, EngineDataModel& model);

    // Opens UART1 with the pins/baud from board_config. No TX pin is
    // attached, so the link is read-only at the hardware level.
    void begin();
    void begin(uint32_t baud, int8_t rxPin, int8_t txPin);

    // Call as often as possible; the EMU stream is continuous.
    void loop(uint32_t nowMs);

    uint32_t bytesConsumed() const { return adapter_.bytesConsumed(); }

    const EmuLog& log() const { return log_; }

private:
    HardwareSerial& uart_;
    EngineDataModel& model_;

    // The log taps the byte stream on its way into the decoder; see
    // EmuLogTap for why it cannot simply be a call in poll().
    EmuLog log_;
    EmuLogTap tap_;

    EmuSerialAdapter adapter_;
};

}  // namespace ecu
