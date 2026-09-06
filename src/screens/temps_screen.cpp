#include "temps_screen.hpp"

#include "dash_theme.hpp"

namespace ecu {
namespace {
constexpr lv_coord_t kCol = 160;
constexpr lv_coord_t kRow = 143;
}  // namespace

void TempsScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    clt_.create(root_, 0, 0, kCol, kRow, "CLT", "C", &lv_font_montserrat_48);
    iat_.create(root_, kCol, 0, kCol, kRow, "IAT", "C", &lv_font_montserrat_48);
    ecuTemp_.create(root_, kCol * 2, 0, kCol, kRow, "ECU T", "C", &lv_font_montserrat_48);

    oil_.create(root_, 0, kRow, kCol, kRow, "OIL P", "bar", &lv_font_montserrat_48);
    fuel_.create(root_, kCol, kRow, kCol, kRow, "FUEL P", "bar", &lv_font_montserrat_48);
    deltaFpr_.create(root_, kCol * 2, kRow, kCol, kRow, "DFPR", "kPa", &lv_font_montserrat_48);
}

void TempsScreen::update(const EngineDataModel& model,
                         const AlarmEngine& alarms,
                         const RunPeaks& peaks,
                         uint32_t nowMs) {
    (void)peaks;
    (void)nowMs;
    const EngineSnapshot& s = model.snapshot();

    clt_.setInt(s.cltC);
    iat_.setInt(s.iatC);
    ecuTemp_.setInt(s.ecuTempC);
    oil_.setFloat(s.oilPressureBar, 1);
    fuel_.setFloat(s.fuelPressureBar, 1);
    deltaFpr_.setInt(s.deltaFprKpa);

    clt_.setSeverity(alarms.severity(AlarmId::Coolant));
    iat_.setSeverity(alarms.severity(AlarmId::IntakeAir));
    oil_.setSeverity(alarms.severity(AlarmId::OilPressure));
    fuel_.setSeverity(alarms.severity(AlarmId::FuelPressure));
}

}  // namespace ecu
