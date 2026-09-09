#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ecu {

// Somewhere for an adapter to hand a raw frame that passed its framing checks.
//
// This exists so the protocol layer can offer frames to the logger without
// knowing what a logger is. The alternative - having the adapter hold an
// `EmuLog&` - would point the protocol layer at the storage layer, and the
// whole reason there is an adapter layer is that it does not do that.
//
// The frame arrives exactly as it came off the wire, marker included, before
// anything has been decoded or scaled. That ordering is the point: the log
// file is the wire bytes, so a decode bug cannot reach it. It also means a
// sink must not assume the frame outlives the call.
class FrameSink {
public:
    virtual ~FrameSink() = default;
    virtual void onFrame(const uint8_t* frame, size_t size) = 0;
};

}  // namespace ecu
