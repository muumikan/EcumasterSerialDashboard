#pragma once

#include <Arduino.h>
#include <EDLSerial.h>

#include "engine_data.hpp"

namespace ecu {

// Frame marker at the head of every EDL-1 frame.
constexpr uint8_t kEdlMagic[4] = {0x32, 0x40, 0x50, 0x60};
constexpr size_t kEdlMagicSize = sizeof(kEdlMagic);
constexpr size_t kEdlFrameSize = 260;

// Adapter for the EMU's EDL-1 logger stream.
//
// The EDL-1 protocol sends every channel at once - 260 bytes carrying 195
// values from a single instant, at 115200 baud - where the classic protocol
// sends one channel per 5-byte frame. Complete samples are what a log file
// needs, and what a dashboard wants anyway.
//
// The protocol carries no checksum, so this class has to earn its confidence
// from framing alone, in three steps:
//
//  1. Slide a one-byte window until the marker lands at the head. The vendored
//     library instead discards all 260 buffered bytes on a mismatch, which
//     preserves the phase error - a listener that starts mid-frame, which is
//     every listener, can stay misaligned indefinitely.
//
//  2. Do not accept a frame until the *next* marker has been seen exactly 260
//     bytes later. A frame spliced together from two partial ones passes the
//     head check but fails this, which is what catches a dropped byte. It
//     costs one frame of latency, about 22 ms.
//
//  3. Reject frames whose values cannot be real, and keep the last good frame
//     rather than the last frame. One bad sample is otherwise permanent: it
//     poisons the run peaks for the rest of the drive.
//
// Read-only, like the classic adapter: nothing here ever writes to the stream.
class EdlSerialAdapter {
public:
    explicit EdlSerialAdapter(Stream& stream);

    // Drain the UART and decode any complete frames. Returns bytes consumed.
    uint32_t poll();

    // Translate the last frame that passed every check.
    EngineSnapshot read() const;

    // That same frame, for values the snapshot does not carry yet.
    const EDLFrame& frame() const { return lastGood_; }

    uint32_t bytesConsumed() const { return bytesConsumed_; }
    uint32_t framesDecoded() const { return frames_; }

    // Frames thrown away: the marker was not where the next one should have
    // been, or a value was impossible. Both mean bytes were lost upstream.
    uint32_t badFrames() const { return splices_ + rejected_; }
    uint32_t splicedFrames() const { return splices_; }
    uint32_t rejectedFrames() const { return rejected_; }

    // Frames whose frameStamp did not advance plausibly. Counted only - the
    // field's exact meaning is not documented anywhere this project could
    // check, so it is a hint, never a verdict.
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

    bool markerAt(size_t offset) const;
    bool headCouldBeMarker() const;
    void realign();
    void acceptFrame();

    Stream& stream_;
    FramePump pump_;
    EDLSerial edl_;

    // One frame plus the lookahead needed to confirm the next marker.
    uint8_t buffer_[kEdlFrameSize + kEdlMagicSize] = {};
    size_t fill_ = 0;

    EDLFrame lastGood_ = {};
    bool haveGood_ = false;

    uint32_t bytesConsumed_ = 0;
    uint32_t frames_ = 0;
    uint32_t splices_ = 0;
    uint32_t rejected_ = 0;
    uint32_t suspect_ = 0;
    uint16_t lastStamp_ = 0;
    bool haveStamp_ = false;
};

}  // namespace ecu
