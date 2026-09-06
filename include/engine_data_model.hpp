#pragma once

#include <stdint.h>

#include "engine_data.hpp"

namespace ecu {

// No fresh data for this long -> values are shown, but flagged as stale.
constexpr uint32_t kLinkStaleTimeoutMs = 500;

// No fresh data for this long -> the link is considered dead.
constexpr uint32_t kLinkOfflineTimeoutMs = 3000;

// Single source of truth for the UI.
//
// Holds the latest snapshot plus link health, and knows nothing about UART,
// framing or the EMU protocol. The UI only ever reads from here.
class EngineDataModel {
public:
    // Called by the provider whenever a decode pass produced data.
    void applySnapshot(const EngineSnapshot& snapshot, uint32_t nowMs);

    const EngineSnapshot& snapshot() const { return snapshot_; }

    LinkState linkState(uint32_t nowMs) const;

    // Milliseconds since the last accepted update. UINT32_MAX before the
    // first one.
    uint32_t ageMs(uint32_t nowMs) const;

    // Bumped only when at least one value actually changed, so the UI can
    // skip redraws cheaply.
    uint32_t revision() const { return revision_; }

    // Total number of accepted updates, for diagnostics.
    uint32_t updateCount() const { return updates_; }

private:
    EngineSnapshot snapshot_{};
    uint32_t lastUpdateMs_ = 0;
    uint32_t revision_ = 0;
    uint32_t updates_ = 0;
    bool everUpdated_ = false;
};

}  // namespace ecu
