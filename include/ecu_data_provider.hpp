#pragma once

#include <Arduino.h>

#include "ecu_link.hpp"
#include "emu_log.hpp"
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
    //
    // Does not open the log: the log is named after the date, and the clock
    // has not necessarily been read this early. AppController opens it.
    void begin();
    void begin(uint32_t baud, int8_t rxPin, int8_t txPin);

    // Call as often as possible; the EMU stream is continuous.
    void loop(uint32_t nowMs);

    uint32_t bytesConsumed() const { return adapter_.bytesConsumed(); }

    EmuLog& log() { return log_; }
    const EmuLog& log() const { return log_; }

private:
    HardwareSerial& uart_;
    EngineDataModel& model_;

    // The adapter offers whole validated frames to the log; see its setSink.
    // Nothing taps the byte stream any more, because nothing needs to: the
    // .emulog record is an EDL-1 frame minus its marker, and the adapter has
    // already done the work of deciding where a frame begins.
    EmuLog log_;
    EcuLinkAdapter adapter_;
};

}  // namespace ecu
