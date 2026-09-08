#pragma once

#include "dash_settings.hpp"

namespace ecu {

// Persists DashSettings in NVS as one versioned blob.
//
// One blob rather than twenty keys: the settings are read and written together,
// and a single record cannot end up half-migrated after a firmware change.
class SettingsStore {
public:
    // Fills `out` from flash. Returns false if nothing usable was stored, in
    // which case `out` holds the defaults.
    bool load(DashSettings& out);

    void save(const DashSettings& in);

    // Wipes the stored record so the next boot comes up on defaults.
    void clear();
};

}  // namespace ecu
