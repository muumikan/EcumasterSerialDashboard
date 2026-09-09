#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "date_time.hpp"
#include "frame_sink.hpp"

namespace ecu {

// Writes an Ecumaster .emulog file to microSD.
//
// The format is documented in docs/emu-log-format.md and was established from
// Client-written logs, then proven by building a file that EMU Classic Client
// opens. In short:
//
//     .emulog = gzip( 12-byte header + N x 256-byte records )
//     record[i] == EDL-1 frame data[i+4]
//
// A record is the 260-byte EDL-1 frame with its 4-byte marker stripped. So
// this class decodes nothing: it takes frames the adapter has already framed,
// drops four bytes, and compresses. No channel map, no scaling, and no way for
// a decode bug to corrupt a log.
//
// Two properties of the format shape the implementation:
//
//  - **gzip is mandatory.** An uncompressed file opens in the Client without
//    an error and graphs nothing. There is no half-measure available.
//
//  - **The stream is never finalised.** The Client's own files end on a
//    Z_SYNC_FLUSH boundary with no gzip trailer, which is how a logger writes
//    a file it may never get to close. So neither does this: pulling the key
//    costs at most the bytes since the last flush, and `end()` exists for
//    tidiness rather than correctness.
//
// EDL-1 only. The classic protocol's 5-byte frames cannot express this format
// at all - see EcuDataProvider, which does not attach a logger in that build.
class EmuLog : public FrameSink {
public:
    // Mounts the card and opens "/YYYYMMDD_HHMM_SS.emulog". The date lives in
    // the filename and nowhere else in the format, so a clock that has not
    // been read yet is a reason to wait rather than to write a wrong name.
    // Safe to call with no card: logging simply stays off.
    bool begin(const DateTime& now);

    // Refuse to log at all, and say why when asked. Used on the classic link,
    // where the format cannot be produced - see EcuDataProvider's constructor.
    // Without this the logger would mount the card and create an empty file
    // that never receives a frame.
    void disable(const char* why);

    // One frame straight off the wire, marker included. From FrameSink.
    void onFrame(const uint8_t* frame, size_t size) override;

    // Periodic flush. Call from the main loop.
    void loop(uint32_t nowMs);

    // Flush and stop. Frames after this are dropped.
    void end();

    bool ready() const { return ready_; }

    // Why logging is off, or nullptr when it is running. The boot messages
    // scroll past before a console can attach, so the reason has to survive
    // somewhere it can be asked for later.
    const char* failure() const { return ready_ ? nullptr : failure_; }

    uint32_t framesWritten() const { return frames_; }
    uint32_t bytesWritten() const { return bytes_; }        // compressed, on card
    uint32_t framesDropped() const { return dropped_; }
    const char* fileName() const { return fileName_; }

private:
    // A card that mounts on one reset and not the next is not a card that is
    // missing. Removable media is allowed a couple of goes.
    static constexpr uint8_t kMountAttempts = 3;
    static constexpr uint32_t kMountRetryMs = 100;

    static constexpr size_t kFrameSize = 260;
    static constexpr size_t kMarkerSize = 4;    // dropped; see the class comment
    static constexpr size_t kRecordSize = kFrameSize - kMarkerSize;

    // Deflate output is accumulated here and written in one go. Sized to a
    // whole flash page so a write is never a read-modify-write.
    //
    // This buffer, not the compressor, sets what a power cut costs: whatever
    // has not reached the card is gone. At the ~1.3 kB/s this format
    // compresses to, 4 KB is about three seconds. Writing more often would
    // mean a few hundred bytes per write and the FAT overhead of each one.
    static constexpr size_t kCardBufferSize = 4096;

    // Sync-flush the compressor this often. This does not push anything to the
    // card; it makes the byte stream readable if it is cut here, which is what
    // lets a truncated file be opened at all. Costs a little compression.
    static constexpr uint8_t kFramesPerFlush = 6;

    // Write to the card at least this often even if the buffer has not filled,
    // so a quiet link does not leave the last records stranded in RAM.
    static constexpr uint32_t kCardFlushIntervalMs = 5000;

    bool openCard();
    bool startStream(const DateTime& now);
    bool compress(const uint8_t* data, size_t size, int flush);
    void appendToCard(const uint8_t* data, size_t size);
    void writeCardBuffer();
    void fail(const char* why);

    // miniz's put-buf callback; forwards to appendToCard.
    static int putBuf(const void* buffer, int length, void* user);

    void* deflator_ = nullptr;   // tdefl_compressor, allocated in PSRAM
    uint8_t cardBuffer_[kCardBufferSize] = {};
    size_t cardFill_ = 0;

    uint32_t frames_ = 0;
    uint32_t dropped_ = 0;
    uint32_t bytes_ = 0;
    uint8_t sinceFlush_ = 0;
    uint32_t lastCardFlushMs_ = 0;
    bool ready_ = false;
    bool disabled_ = false;
    bool writeFailed_ = false;
    const char* failure_ = "not started";
    char fileName_[32] = {0};
};

}  // namespace ecu
