#pragma once

#include <Arduino.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
//  Raw EMU serial frame logging to microSD.
//
//  ATTRIBUTION — the log file format documented and produced here was learned
//  by reading:
//
//      danuecumaster/ECUMaster-ESP32-Bluetooth-Dashboard-Logger
//      https://github.com/danuecumaster/ECUMaster-ESP32-Bluetooth-Dashboard-Logger
//      Licensed GNU GPL v3.0
//      Specifically main.ino: logEMU(), readFrame() and getNextFilename().
//
//  What was taken from it is the *file format*, not its code: validated 5-byte
//  EMU serial frames concatenated verbatim, no header, no footer, no in-file
//  timestamps, in a file named "*.emualog". The implementation below is written
//  from that description plus the frame layout the protocol already dictates,
//  which this project independently implements in EmuSerialAdapter.
//
//  See docs/emu-log-format.md for what is verified and what is not. The claim
//  that Ecumaster's PC software opens this format is that project's claim; it
//  has not been confirmed here against real software or an official spec.
// -----------------------------------------------------------------------------

namespace ecu {

// Log frames as the ECU sent them, byte for byte.
//
// Fed one raw byte at a time from the ECU stream. Re-does the protocol's own
// framing so that only frames with a correct magic byte and checksum reach the
// card - garbage from a resync is dropped rather than written.
class EmuLog {
public:
    // Mounts the card and opens the next free "/NNNNN.emualog". Safe to call
    // when no card is present: logging simply stays off.
    bool begin();

    // One raw byte straight off the ECU link, before any decoding.
    void feed(uint8_t byte);

    // Periodic flush. Call from the main loop.
    void loop(uint32_t nowMs);

    // Flush and close. Frames fed after this are dropped.
    void end();

    bool ready() const { return ready_; }

    // Why logging is off, or nullptr when it is running. The boot messages
    // scroll past before a console can attach, so the reason has to survive
    // somewhere it can be asked for later.
    const char* failure() const { return ready_ ? nullptr : failure_; }
    uint32_t framesWritten() const { return frames_; }
    uint32_t bytesWritten() const { return bytes_; }
    const char* fileName() const { return fileName_; }

private:
    // A card that mounts on one reset and not the next is not a card that is
    // missing. Removable media is allowed a couple of goes.
    static constexpr uint8_t kMountAttempts = 3;
    static constexpr uint32_t kMountRetryMs = 100;

    static constexpr size_t kFrameSize = 5;
    static constexpr size_t kBufferSize = 512;   // whole flash pages at a time
    static constexpr uint32_t kFlushIntervalMs = 5000;

    void writeFrame(const uint8_t* frame);
    void drain();

    uint8_t window_[kFrameSize] = {};
    uint8_t windowFill_ = 0;

    uint8_t buffer_[kBufferSize] = {};
    size_t bufferFill_ = 0;

    uint32_t frames_ = 0;
    uint32_t bytes_ = 0;
    uint32_t lastFlushMs_ = 0;
    bool ready_ = false;
    const char* failure_ = "not started";
    char fileName_[24] = {0};
};

// A read-only pass-through Stream that copies every byte to the log.
//
// This exists because EMUSerial reads the port itself and never exposes the
// bytes it consumed: `checkEmuSerial()` returns void and `decodeEmuFrame()` is
// private. Rather than modify the vendored reference decoder, the adapter is
// pointed at this wrapper instead of the UART, and the bytes are tapped as they
// pass through.
//
// write() is deliberately inert. The ECU link is read-only, and nothing should
// be able to transmit through this object either.
class EmuLogTap : public Stream {
public:
    EmuLogTap(Stream& source, EmuLog& log) : source_(source), log_(log) {}

    int available() override { return source_.available(); }
    int peek() override { return source_.peek(); }

    int read() override {
        const int value = source_.read();
        if (value >= 0) {
            log_.feed(static_cast<uint8_t>(value));
        }
        return value;
    }

    // Never transmits. See the class comment.
    size_t write(uint8_t) override { return 0; }
    void flush() override {}

private:
    Stream& source_;
    EmuLog& log_;
};

}  // namespace ecu
