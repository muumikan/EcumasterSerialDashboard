#include "engine_data_model.hpp"

#include <string.h>

namespace ecu {

const char* toString(LinkState state) {
    switch (state) {
        case LinkState::Online:  return "ONLINE";
        case LinkState::Stale:   return "STALE";
        case LinkState::Offline: return "OFFLINE";
    }
    return "?";
}

void EngineDataModel::applySnapshot(const EngineSnapshot& snapshot, uint32_t nowMs) {
    if (memcmp(&snapshot_, &snapshot, sizeof(EngineSnapshot)) != 0) {
        snapshot_ = snapshot;
        ++revision_;
    }

    lastUpdateMs_ = nowMs;
    ++updates_;
    everUpdated_ = true;
}

uint32_t EngineDataModel::ageMs(uint32_t nowMs) const {
    if (!everUpdated_) {
        return UINT32_MAX;
    }
    return nowMs - lastUpdateMs_;  // unsigned wrap is intentional
}

LinkState EngineDataModel::linkState(uint32_t nowMs) const {
    const uint32_t age = ageMs(nowMs);

    if (age >= kLinkOfflineTimeoutMs) {
        return LinkState::Offline;
    }
    if (age >= kLinkStaleTimeoutMs) {
        return LinkState::Stale;
    }
    return LinkState::Online;
}

}  // namespace ecu
