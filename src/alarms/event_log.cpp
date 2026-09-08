#include "event_log.hpp"

#include <string.h>

namespace ecu {
namespace {

void copyField(char* dest, size_t size, const char* source) {
    if (source == nullptr) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, source, size - 1);
    dest[size - 1] = '\0';
}

}  // namespace

void EventLog::raise(uint16_t key,
                     const DateTime& time,
                     const char* label,
                     const char* value,
                     uint16_t rpm,
                     AlarmSeverity severity,
                     uint32_t nowMs) {
    AlarmEvent* open = findOpen(key);
    if (open != nullptr) {
        // The condition is already on the list. Escalating a warning to a
        // critical updates that entry; a value drifting further past a limit
        // it has already crossed is not news.
        if (severity > open->severity) {
            open->severity = severity;
            copyField(open->value, sizeof(open->value), value);
            ++revision_;
        }
        return;
    }

    AlarmEvent& slot = events_[head_];
    slot = AlarmEvent{};
    slot.time = time;
    copyField(slot.label, sizeof(slot.label), label);
    copyField(slot.value, sizeof(slot.value), value);
    slot.key = key;
    slot.rpm = rpm;
    slot.startMs = nowMs;
    slot.severity = severity;
    slot.active = true;

    head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
    if (count_ < kCapacity) {
        ++count_;
    }
    ++total_;
    ++revision_;
}

void EventLog::clear(uint16_t key, uint32_t nowMs) {
    AlarmEvent* open = findOpen(key);
    if (open == nullptr) {
        return;
    }
    open->active = false;
    open->endMs = nowMs;
    ++revision_;
}

void EventLog::reset() {
    count_ = 0;
    head_ = 0;
    total_ = 0;
    ++revision_;
}

const AlarmEvent& EventLog::at(uint8_t index) const {
    const uint8_t slot =
        static_cast<uint8_t>((head_ + kCapacity - 1 - (index % kCapacity)) % kCapacity);
    return events_[slot];
}

uint8_t EventLog::activeCount() const {
    uint8_t open = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (at(i).active) {
            ++open;
        }
    }
    return open;
}

void EventLog::stampRunStart(const DateTime& time) {
    if (!runStart_.valid && time.valid) {
        runStart_ = time;
    }
}

AlarmEvent* EventLog::findOpen(uint16_t key) {
    for (uint8_t i = 0; i < count_; ++i) {
        const uint8_t slot =
            static_cast<uint8_t>((head_ + kCapacity - 1 - i) % kCapacity);
        if (events_[slot].active && events_[slot].key == key) {
            return &events_[slot];
        }
    }
    return nullptr;
}

}  // namespace ecu
