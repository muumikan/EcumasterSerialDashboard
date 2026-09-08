#pragma once

#include <stdint.h>

#include "alarm_severity.hpp"
#include "date_time.hpp"

namespace ecu {

// One thing that went wrong, with the moment it happened.
//
// This is a record of an event, not the state of a signal: a coolant alarm
// that trips twice in a run leaves two of these, in the order they happened.
struct AlarmEvent {
    DateTime time;          // wall clock when the condition first became true
    char label[10] = {0};   // "OIL P", "CEL", "LINK"
    char value[12] = {0};   // "1.4 bar", "IAT", "OFFLINE"
    uint16_t key = 0;       // identifies the condition, so it can be closed
    uint16_t rpm = 0;       // engine speed at onset
    uint32_t startMs = 0;
    uint32_t endMs = 0;     // meaningless while active
    AlarmSeverity severity = AlarmSeverity::None;
    bool active = false;    // the condition is still true right now
};

// Keys. The source is encoded in the high byte so that a coolant limit and a
// coolant-sensor check-engine bit cannot close each other's events.
constexpr uint16_t kLimitKeyBase = 0x0000;
constexpr uint16_t kCelKeyBase = 0x0100;
constexpr uint16_t kLinkKey = 0x0200;

// A fixed ring of the most recent events.
//
// Deliberately not acknowledged and not cleared by hand: this is a log the
// driver reads, not a queue an operator works through. The oldest entry falls
// off the end when the ring is full, and the whole thing is per-run.
class EventLog {
public:
    static constexpr uint8_t kCapacity = 32;

    // Records the onset of a condition. If that condition is already open, the
    // existing entry is escalated rather than duplicated - a warning that
    // becomes critical is one event, not two.
    void raise(uint16_t key,
               const DateTime& time,
               const char* label,
               const char* value,
               uint16_t rpm,
               AlarmSeverity severity,
               uint32_t nowMs);

    // Closes the open event with this key, if there is one.
    void clear(uint16_t key, uint32_t nowMs);

    void reset();

    // Newest first: at(0) is the most recent event.
    uint8_t count() const { return count_; }
    const AlarmEvent& at(uint8_t index) const;

    uint8_t activeCount() const;

    // Every event this run, including any the ring has already dropped.
    uint32_t total() const { return total_; }

    // Bumped on every raise and clear, so a screen can skip repainting.
    uint32_t revision() const { return revision_; }

    // The first wall-clock time seen this run. Shown as the foot of the list.
    void stampRunStart(const DateTime& time);
    const DateTime& runStart() const { return runStart_; }

private:
    AlarmEvent* findOpen(uint16_t key);

    AlarmEvent events_[kCapacity];
    DateTime runStart_;
    uint8_t count_ = 0;
    uint8_t head_ = 0;  // next slot to write
    uint32_t total_ = 0;
    uint32_t revision_ = 0;
};

}  // namespace ecu
