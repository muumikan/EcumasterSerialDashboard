#pragma once

#include <Arduino.h>
#include <EMUSerial.h>

#include "engine_data.hpp"

namespace ecu {

// Thin wrapper around the GTO2013/EMUSerial reference decoder.
//
// This is the only place in the project that includes EMUSerial.h, and the
// only place that knows about the EMU wire format. It is strictly read-only:
// no method ever writes to the stream.
class EmuSerialAdapter {
public:
    explicit EmuSerialAdapter(Stream& stream);

    // Drain the UART and let the reference decoder handle any complete frames.
    // Returns the number of bytes consumed, which is used as the liveness
    // signal for the link (EMUSerial itself reports no frame counter).
    uint32_t poll();

    // Translate the decoded protocol struct into the model's snapshot type.
    EngineSnapshot read() const;

    uint32_t bytesConsumed() const { return bytesConsumed_; }

    // Always zero: the vendored decoder reports neither frames nor failures,
    // so the classic link has no bad-frame count to give. Present so both
    // adapters offer the same interface.
    uint32_t badFrames() const { return 0; }

private:
    Stream& stream_;
    EMUSerial emu_;
    uint32_t bytesConsumed_ = 0;
};

}  // namespace ecu
