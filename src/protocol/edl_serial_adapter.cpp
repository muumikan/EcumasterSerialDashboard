#include "edl_serial_adapter.hpp"

#include <string.h>

namespace ecu {
namespace {

// A frameStamp that jumps further than this between consecutive frames is
// counted as suspect. Generous on purpose: whether the field counts frames or
// milliseconds is not documented, and dropped frames widen the gap either way.
constexpr uint16_t kMaxPlausibleStampStep = 512;

// Values that cannot be real, so a frame carrying one was assembled from the
// wrong bytes. The first three limits are Ecumaster's own `maxLimit`
// attributes from the 1.211 definition; the rest are deliberately loose bounds
// chosen here, wide enough that no running engine approaches them.
bool plausible(const EDLFrame& f) {
    if (f.RPM > 15000) return false;            // maxLimit, 1.211
    if (f.MAP > 600) return false;              // maxLimit, 1.211
    if (f.wboLambda < 0.0f || f.wboLambda > 2.0f) return false;   // maxLimit, 1.211
    if (f.CLT < -60 || f.CLT > 250) return false;
    if (f.Batt < 0.0f || f.Batt > 25.0f) return false;
    if (f.oilPressure < -1.0f || f.oilPressure > 20.0f) return false;
    if (f.TPS > 100) return false;
    return true;
}

}  // namespace

EdlSerialAdapter::EdlSerialAdapter(Stream& stream) : stream_(stream) {
    edl_.begin(pump_);
}

bool EdlSerialAdapter::markerAt(size_t offset) const {
    return memcmp(buffer_ + offset, kEdlMagic, kEdlMagicSize) == 0;
}

// True while the bytes collected so far are still consistent with the marker.
bool EdlSerialAdapter::headCouldBeMarker() const {
    const size_t checked = fill_ < kEdlMagicSize ? fill_ : kEdlMagicSize;
    return memcmp(buffer_, kEdlMagic, checked) == 0;
}

// Slide one byte at a time until the head could be a marker. This is the part
// the vendored library gets wrong; see the header.
void EdlSerialAdapter::realign() {
    while (fill_ > 0 && !headCouldBeMarker()) {
        memmove(buffer_, buffer_ + 1, --fill_);
    }
}

void EdlSerialAdapter::acceptFrame() {
    pump_.load(buffer_, kEdlFrameSize);
    if (!edl_.update()) {
        return;  // cannot happen with an aligned frame, but do not assume it
    }

    const EDLFrame& parsed = edl_.getFrame();

    if (!plausible(parsed)) {
        ++rejected_;
        return;   // lastGood_ keeps standing; a bad sample never reaches peaks
    }

    ++frames_;
    lastGood_ = parsed;
    haveGood_ = true;

    // A hint, not a verdict. See the header.
    const uint16_t stamp = parsed.frameStamp;
    if (haveStamp_) {
        const uint16_t step = static_cast<uint16_t>(stamp - lastStamp_);
        if (step == 0 || step > kMaxPlausibleStampStep) {
            ++suspect_;
        }
    }
    lastStamp_ = stamp;
    haveStamp_ = true;
}

uint32_t EdlSerialAdapter::poll() {
    uint32_t consumed = 0;

    while (stream_.available() > 0) {
        const int value = stream_.read();
        if (value < 0) {
            break;
        }
        ++consumed;

        buffer_[fill_++] = static_cast<uint8_t>(value);
        realign();

        if (fill_ < kEdlFrameSize + kEdlMagicSize) {
            continue;
        }

        // A whole frame plus the head of the next one. If the next marker is
        // not exactly where it must be, these 260 bytes were spliced out of
        // two partial frames - drop them and resynchronise.
        if (markerAt(kEdlFrameSize)) {
            acceptFrame();
            memmove(buffer_, buffer_ + kEdlFrameSize, kEdlMagicSize);
            fill_ = kEdlMagicSize;
        } else {
            ++splices_;
            memmove(buffer_, buffer_ + 1, --fill_);
            realign();
        }
    }

    bytesConsumed_ += consumed;
    return consumed;
}

EngineSnapshot EdlSerialAdapter::read() const {
    // Value-initialised so padding bytes are zeroed - the model compares
    // snapshots with memcmp to detect real changes.
    EngineSnapshot s{};
    const EDLFrame& d = lastGood_;

    s.rpm = d.RPM;
    s.mapKpa = d.MAP;
    s.tpsPct = d.TPS;
    s.iatC = d.IAT;
    s.cltC = d.CLT;
    s.baroKpa = d.Baro;
    s.batteryV = d.Batt;

    s.ignAngleDeg = d.IgnAngle;
    s.injPulseWidthMs = d.pulseWidth;
    s.injPulseWidth2Ms = d.scondarypulseWidth;  // sic, spelling from the library
    s.injDutyPct = d.injDC;
    s.dwellTimeMs = d.dwellTime;

    s.wboLambda = d.wboLambda;
    s.wboAfr = d.wboAFR;
    s.lambdaTarget = d.lambdaTarget;
    s.afrTarget = d.afrTarget;   // present here; the 1.211 classic stream drops it

    s.egt1C = d.Egt1;
    s.egt2C = d.Egt2;
    s.knockLevelV = d.knockLevel;

    s.oilPressureBar = d.oilPressure;
    s.oilTempC = d.oilTemperature;
    s.fuelPressureBar = d.fuelPressure;
    s.deltaFprKpa = d.deltaFPR;
    s.fuelLevelPct = d.fuelLevel;

    s.ethanolPct = d.flexFuelEthanolContent;
    s.ethanolTempC = d.ffTemp;

    s.analogIn1V = d.analogIn1;
    s.analogIn2V = d.analogIn2;
    s.analogIn3V = d.analogIn3;
    s.analogIn4V = d.analogIn4;

    s.speedKph = d.vssSpeed;
    s.gear = d.gear;
    s.ecuTempC = d.emuTemp;
    s.tablesSet = d.tablesSet;
    s.celFlags = d.cel;

    return s;
}

}  // namespace ecu
