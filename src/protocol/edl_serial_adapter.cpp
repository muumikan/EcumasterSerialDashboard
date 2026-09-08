#include "edl_serial_adapter.hpp"

#include <string.h>

namespace ecu {
namespace {

// A frameStamp that jumps further than this between consecutive frames is
// counted as suspect. Generous on purpose: whether the field counts frames or
// milliseconds is not documented, and dropped frames widen the gap either way.
constexpr uint16_t kMaxPlausibleStampStep = 512;

}  // namespace

EdlSerialAdapter::EdlSerialAdapter(Stream& stream) : stream_(stream) {
    edl_.begin(pump_);
}

// True while the bytes collected so far are still consistent with the marker.
bool EdlSerialAdapter::markerCouldMatch() const {
    const size_t checked = fill_ < sizeof(kEdlMagic) ? fill_ : sizeof(kEdlMagic);
    return memcmp(buffer_, kEdlMagic, checked) == 0;
}

void EdlSerialAdapter::acceptFrame() {
    pump_.load(buffer_, kEdlFrameSize);
    if (!edl_.update()) {
        return;  // cannot happen with an aligned frame, but do not assume it
    }
    ++frames_;

    // The only integrity check the protocol allows. Counted, never enforced.
    const uint16_t stamp = edl_.getFrame().frameStamp;
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

        // Slide one byte at a time until the marker sits at the head. This is
        // the part the vendored library gets wrong; see the header.
        while (fill_ > 0 && !markerCouldMatch()) {
            memmove(buffer_, buffer_ + 1, --fill_);
        }

        if (fill_ == kEdlFrameSize) {
            acceptFrame();
            fill_ = 0;
        }
    }

    bytesConsumed_ += consumed;
    return consumed;
}

EngineSnapshot EdlSerialAdapter::read() const {
    // Value-initialised so padding bytes are zeroed - the model compares
    // snapshots with memcmp to detect real changes.
    EngineSnapshot s{};
    const EDLFrame& d = edl_.getFrame();

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
