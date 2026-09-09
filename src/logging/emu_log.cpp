#include "emu_log.hpp"

// See the attribution block in emu_log.hpp: the file format produced here was
// learned from a GPLv3 project. No code was copied from it.

#include <SD.h>
#include <SPI.h>
#include <stdio.h>
#include <string.h>

#include "board_config.hpp"

namespace ecu {
namespace {

// The protocol's frame marker. Must match EMUSERIAL_MAGIC in the vendored
// decoder; defined locally so the logger does not pull in EMUSerial.h.
constexpr uint8_t kEmuMagic = 0xA3;

// The SD slot is on its own SPI bus, separate from the display's SPI2.
SPIClass sdSpi(HSPI);

}  // namespace

bool EmuLog::begin() {
    sdSpi.begin(board::kSdSckPin, board::kSdMisoPin, board::kSdMosiPin);

    if (!SD.begin(board::kSdCsPin, sdSpi, board::kSdSpiHz)) {
        // SD.begin covers both "nothing in the slot" and "the card is there
        // but the filesystem will not mount". The second is the common one on
        // a card straight out of its packaging: anything 64 GB or larger ships
        // exFAT, which this library does not read.
        failure_ = "no card, or not FAT32 (exFAT is not supported)";
        Serial.print(F("emulog: "));
        Serial.println(failure_);
        sdSpi.end();
        return false;
    }

    // Sequential names, so no real-time clock is required. The board has an
    // RTC on the touch I2C bus; if it is ever used, timestamped names would be
    // the better scheme.
    for (uint32_t index = 1; index <= 99999; ++index) {
        snprintf(fileName_, sizeof(fileName_), "/%05lu.emualog",
                 static_cast<unsigned long>(index));
        if (!SD.exists(fileName_)) {
            break;
        }
    }

    File file = SD.open(fileName_, FILE_WRITE);
    if (!file) {
        failure_ = "card mounted but the log file could not be created";
        Serial.print(F("emulog: "));
        Serial.println(failure_);
        return false;
    }
    file.close();

    ready_ = true;
    failure_ = nullptr;
    lastFlushMs_ = millis();

    Serial.print(F("emulog: "));
    Serial.print(static_cast<unsigned long>(SD.cardSize() / (1024ULL * 1024ULL)));
    Serial.print(F(" MB card, logging to "));
    Serial.println(fileName_);
    return true;
}

void EmuLog::feed(uint8_t byte) {
    if (!ready_) {
        return;
    }

    // Same sliding window the protocol requires: shift up, take the new byte as
    // the checksum position, and accept when the magic lines up and the sum
    // matches. Anything else keeps sliding, so resync garbage is never written.
    if (windowFill_ < kFrameSize) {
        window_[windowFill_++] = byte;
        if (windowFill_ < kFrameSize) {
            return;
        }
    } else {
        memmove(window_, window_ + 1, kFrameSize - 1);
        window_[kFrameSize - 1] = byte;
    }

    if (window_[1] != kEmuMagic) {
        return;
    }

    const uint8_t checksum =
        static_cast<uint8_t>(window_[0] + window_[1] + window_[2] + window_[3]);
    if (checksum != window_[4]) {
        return;
    }

    writeFrame(window_);
    windowFill_ = 0;  // a clean frame ends the window; start the next one fresh
}

void EmuLog::writeFrame(const uint8_t* frame) {
    if (bufferFill_ + kFrameSize > kBufferSize) {
        drain();
    }

    memcpy(&buffer_[bufferFill_], frame, kFrameSize);
    bufferFill_ += kFrameSize;
    ++frames_;
}

void EmuLog::drain() {
    if (bufferFill_ == 0) {
        return;
    }

    File file = SD.open(fileName_, FILE_APPEND);
    if (!file) {
        Serial.println(F("emulog: append failed, logging disabled"));
        ready_ = false;
        bufferFill_ = 0;
        return;
    }

    const size_t written = file.write(buffer_, bufferFill_);
    file.close();

    bytes_ += written;
    if (written != bufferFill_) {
        Serial.println(F("emulog: short write, card may be full"));
        ready_ = false;
    }
    bufferFill_ = 0;
}

void EmuLog::loop(uint32_t nowMs) {
    if (!ready_) {
        return;
    }
    if (nowMs - lastFlushMs_ < kFlushIntervalMs) {
        return;
    }
    lastFlushMs_ = nowMs;
    drain();
}

void EmuLog::end() {
    if (!ready_) {
        return;
    }
    drain();
    ready_ = false;
}

}  // namespace ecu
