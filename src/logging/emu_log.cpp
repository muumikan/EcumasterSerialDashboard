#include "emu_log.hpp"

#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "board_config.hpp"
#include "rom/miniz.h"

namespace ecu {
namespace {

// The SD slot is on its own SPI bus, separate from the display's SPI2.
SPIClass sdSpi(HSPI);

// Held open for the life of the log. There is exactly one EmuLog, and the bus
// above is a file-static for the same reason, so this is not hiding a second
// instance. Keeping it open matters: reopening to append every few seconds is
// tens of milliseconds inside the UI's frame budget, and flushing after each
// write persists the directory entry anyway, so closing buys nothing.
File logFile;

// The gzip member header the Client writes: deflate, no flags, no mtime, no
// extra fields, OS 0x0b. Ten bytes, identical in every sample seen.
constexpr uint8_t kGzipHeader[] = {0x1F, 0x8B, 0x08, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x0B};

// The .emulog header, which lives inside the compressed stream. Marker,
// version, and a third word whose purpose is unknown - constant in every
// sample, so it is reproduced rather than understood.
constexpr uint8_t kLogHeader[] = {0x80, 0x60, 0x40, 0x20, 0x01, 0x00,
                                  0x00, 0x00, 0x80, 0x96, 0x98, 0x00};

// Raw deflate. TDEFL_WRITE_ZLIB_HEADER is deliberately absent: the gzip header
// above is written by hand, and a zlib header underneath it would be a second,
// wrong one.
constexpr int kDeflateFlags = TDEFL_DEFAULT_MAX_PROBES;

}  // namespace

// miniz's ROM build calls this with whatever deflate produced. Returning
// MZ_FALSE would abort the compressor mid-stream; a card fault is handled in
// writeCardBuffer instead, where there is something useful to say about it.
int EmuLog::putBuf(const void* buffer, int length, void* user) {
    static_cast<EmuLog*>(user)->appendToCard(static_cast<const uint8_t*>(buffer),
                                             static_cast<size_t>(length));
    return MZ_TRUE;
}

void EmuLog::fail(const char* why) {
    failure_ = why;
    ready_ = false;
    if (logFile) {
        logFile.close();
    }
    Serial.print(F("emulog: "));
    Serial.println(why);
}

bool EmuLog::openCard() {
    sdSpi.begin(board::kSdSckPin, board::kSdMisoPin, board::kSdMosiPin);

    // Measured on this board: the same card mounts on one reset and fails on
    // the next, about half the time, and lowering the bus clock from 80 to
    // 25 MHz did not change that. It is not the card and not the wiring - it
    // is initialisation timing, which is what retries are for.
    for (uint8_t attempt = 1; attempt <= kMountAttempts; ++attempt) {
        if (SD.begin(board::kSdCsPin, sdSpi, board::kSdSpiHz)) {
            if (attempt > 1) {
                Serial.print(F("emulog: card mounted on attempt "));
                Serial.println(attempt);
            }
            return true;
        }
        SD.end();
        delay(kMountRetryMs);
    }

    sdSpi.end();
    return false;
}

bool EmuLog::startStream(const DateTime& now) {
    // The date lives in the filename and nowhere else in the format. This is
    // the one thing the writer contributes that the ECU does not - the session
    // name below is the other.
    const char* tail = session_[0] != '\0' ? "_" : "";
    snprintf(fileName_, sizeof(fileName_), "/%04u%02u%02u_%02u%02u_%02u%s%s.emulog",
             now.year, now.month, now.day, now.hour, now.minute, now.second,
             tail, session_);

    // Two power cycles inside the same second collide, and so does a
    // build-time fallback name used twice. FILE_WRITE truncates, so a
    // collision would quietly destroy the earlier drive.
    for (uint8_t suffix = 1; suffix < 100 && SD.exists(fileName_); ++suffix) {
        snprintf(fileName_, sizeof(fileName_), "/%04u%02u%02u_%02u%02u_%02u%s%s_%u.emulog",
                 now.year, now.month, now.day, now.hour, now.minute, now.second,
                 tail, session_, suffix);
    }

    logFile = SD.open(fileName_, FILE_WRITE);
    if (!logFile) {
        return false;
    }

    // About 164 KB with the ROM's TDEFL_LESS_MEMORY build. That does not fit
    // in internal RAM beside LVGL's buffers, and there is 8 MB of PSRAM.
    if (deflator_ == nullptr) {
        deflator_ = heap_caps_malloc(sizeof(tdefl_compressor), MALLOC_CAP_SPIRAM);
    }
    if (deflator_ == nullptr) {
        logFile.close();
        return false;
    }
    tdefl_init(static_cast<tdefl_compressor*>(deflator_), putBuf, this, kDeflateFlags);

    // The gzip header goes to the card as-is; everything after it is deflate
    // output, starting with the log header.
    appendToCard(kGzipHeader, sizeof(kGzipHeader));
    return compress(kLogHeader, sizeof(kLogHeader), TDEFL_NO_FLUSH);
}

void EmuLog::disable(const char* why) {
    disabled_ = true;
    failure_ = why;
}

void EmuLog::enable() {
    if (ready_) {
        return;
    }
    disabled_ = false;
    failure_ = "not started";
}

bool EmuLog::begin(const DateTime& now) {
    if (disabled_) {
        return false;
    }
    if (!now.valid) {
        failure_ = "waiting for the clock";
        return false;   // not a failure yet; the caller retries
    }
    if (!openCard()) {
        // Still both possibilities after three tries: nothing in the slot, or
        // a filesystem this library cannot read. Anything 64 GB or larger
        // ships exFAT, which it cannot.
        fail("no card, or not FAT32 (exFAT is not supported)");
        return false;
    }
    if (!startStream(now)) {
        fail("could not open the log file");
        return false;
    }

    ready_ = true;
    writeFailed_ = false;
    lastCardFlushMs_ = millis();

    Serial.print(F("emulog: logging to "));
    Serial.println(fileName_);
    return true;
}

void EmuLog::setSessionName(const char* name) {
    size_t out = 0;
    for (size_t i = 0; name != nullptr && name[i] != '\0' && out + 1 < sizeof(session_); ++i) {
        const char c = name[i];
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '_' || c == '-';
        session_[out++] = safe ? c : '-';
    }
    session_[out] = '\0';
}

// Not begin() again: the card stays mounted across the swap, and the
// compressor is re-initialised in place rather than reallocated. Only the file
// changes.
bool EmuLog::rotate(const DateTime& now) {
    if (disabled_ || !now.valid) {
        return false;
    }

    end();

    if (!startStream(now)) {
        fail("could not open the next log file");
        return false;
    }

    // The counters describe the open file, and this is a different file.
    frames_ = 0;
    dropped_ = 0;
    bytes_ = 0;
    sinceFlush_ = 0;
    ready_ = true;
    writeFailed_ = false;
    lastCardFlushMs_ = millis();

    Serial.print(F("emulog: rotated to "));
    Serial.println(fileName_);
    return true;
}

bool EmuLog::compress(const uint8_t* data, size_t size, int flush) {
    const tdefl_status status =
        tdefl_compress_buffer(static_cast<tdefl_compressor*>(deflator_), data, size,
                              static_cast<tdefl_flush>(flush));
    return status == TDEFL_STATUS_OKAY || status == TDEFL_STATUS_DONE;
}

void EmuLog::appendToCard(const uint8_t* data, size_t size) {
    while (size > 0) {
        const size_t room = kCardBufferSize - cardFill_;
        const size_t take = size < room ? size : room;
        memcpy(&cardBuffer_[cardFill_], data, take);
        cardFill_ += take;
        data += take;
        size -= take;
        if (cardFill_ == kCardBufferSize) {
            writeCardBuffer();
        }
    }
}

void EmuLog::writeCardBuffer() {
    if (cardFill_ == 0 || writeFailed_) {
        cardFill_ = 0;
        return;
    }

    const size_t written = logFile.write(cardBuffer_, cardFill_);
    logFile.flush();   // persists the directory entry, so a power cut keeps it

    bytes_ += written;
    const bool short_write = written != cardFill_;
    cardFill_ = 0;
    if (short_write) {
        writeFailed_ = true;
        fail("short write, card may be full");
    }
}

void EmuLog::onFrame(const uint8_t* frame, size_t size) {
    if (!ready_) {
        ++dropped_;
        return;
    }
    if (size != kFrameSize) {
        ++dropped_;   // cannot happen with an aligned frame; do not assume it
        return;
    }

    // Every kFramesPerFlush frames the deflate stream is sync-flushed, which
    // makes that point a place the file can be cut and still read. It is the
    // only thing standing between a power cut and the whole drive.
    ++sinceFlush_;
    const bool flushing = sinceFlush_ >= kFramesPerFlush;
    if (flushing) {
        sinceFlush_ = 0;
    }

    // Drop the 4-byte marker. What is left is the record, byte for byte.
    if (!compress(frame + kMarkerSize, kRecordSize,
                  flushing ? TDEFL_SYNC_FLUSH : TDEFL_NO_FLUSH)) {
        fail("the compressor rejected a frame");
        return;
    }
    ++frames_;
}

void EmuLog::loop(uint32_t nowMs) {
    if (!ready_) {
        return;
    }
    if (nowMs - lastCardFlushMs_ < kCardFlushIntervalMs) {
        return;
    }
    lastCardFlushMs_ = nowMs;
    writeCardBuffer();
}

void EmuLog::end() {
    if (!ready_) {
        return;
    }
    // A sync flush, not TDEFL_FINISH. The file is deliberately left without a
    // gzip trailer, exactly as the Client's own logs are - see the header.
    compress(nullptr, 0, TDEFL_SYNC_FLUSH);
    writeCardBuffer();
    ready_ = false;
    failure_ = "stopped";
    if (logFile) {
        logFile.close();
    }
}

}  // namespace ecu
