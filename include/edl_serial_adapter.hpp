#pragma once

#include <Arduino.h>
#include <EDLSerial.h>

#include "engine_data.hpp"

namespace ecu {

// Frame marker at the head of every EDL-1 frame.
constexpr uint8_t kEdlMagic[4] = {0x32, 0x40, 0x50, 0x60};
constexpr size_t kEdlFrameSize = 260;

// Adapter for the EMU's EDL-1 logger stream.
//
// The EDL-1 protocol sends every channel at once - 260 bytes carrying 195
// values from a single instant, at 115200 baud - where the classic protocol
// sends one channel per 5-byte frame. Complete samples are what a log file
// needs, and what a dashboard wants anyway.
//
// Framing is done here rather than in the vendored library, for a specific
// reason. EDLSerial::update() collects 260 bytes, checks the marker, and on a
// mismatch discards the whole buffer. That preserves the phase error: a
// listener that starts mid-frame - which is every listener, since the ECU is
// already streaming when the dash powers up - stays misaligned indefinitely.
// This class slides a window one byte at a time until the marker lands, then
// hands the library a frame it is guaranteed to accept.
//
// Read-only, like the classic adapter: nothing here ever writes to the stream.
class EdlSerialAdapter {
public:
    explicit EdlSerialAdapter(Stream& stream);

    // Drain the UART and decode any complete frames. Returns bytes consumed.
    uint32_t poll();

    // Translate the last decoded frame into the model's snapshot type.
    EngineSnapshot read() const;

    // The whole decoded frame, for values the snapshot does not carry yet.
    const EDLFrame& frame() const { return edl_.getFrame(); }

    uint32_t bytesConsumed() const { return bytesConsumed_; }
    uint32_t framesDecoded() const { return frames_; }

    // Frames whose frameStamp did not advance plausibly. The EDL-1 protocol
    // carries no checksum, so this is the only integrity signal available -
    // and it is a hint, not a verdict: it never rejects a frame, because the
    // exact meaning of frameStamp is not documented anywhere this project has
    // been able to check.
    uint32_t suspectFrames() const { return suspect_; }

private:
    // Feeds one already-aligned frame to the vendored parser.
    class FramePump : public Stream {
    public:
        void load(const uint8_t* data, size_t size) {
            data_ = data;
            size_ = size;
            position_ = 0;
        }
        int available() override { return static_cast<int>(size_ - position_); }
        int read() override { return position_ < size_ ? data_[position_++] : -1; }
        int peek() override { return position_ < size_ ? data_[position_] : -1; }
        size_t write(uint8_t) override { return 0; }   // never transmits
        void flush() override {}

    private:
        const uint8_t* data_ = nullptr;
        size_t size_ = 0;
        size_t position_ = 0;
    };

    bool markerCouldMatch() const;
    void acceptFrame();

    Stream& stream_;
    FramePump pump_;
    EDLSerial edl_;

    uint8_t buffer_[kEdlFrameSize] = {};
    size_t fill_ = 0;

    uint32_t bytesConsumed_ = 0;
    uint32_t frames_ = 0;
    uint32_t suspect_ = 0;
    uint16_t lastStamp_ = 0;
    bool haveStamp_ = false;
};

}  // namespace ecu
