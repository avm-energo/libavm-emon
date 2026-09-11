#include <libavm-emon/Calibration.h>

namespace avm_emon {

double calibratedCurrent(double rawAmps, const SupportedLimits &limits, std::uint16_t currentLimitType,
                          std::uint16_t currentLimitIndex)
{
    const QVector<double> &table = limits.currentListFor(currentLimitType);
    return rawAmps * table.at(currentLimitIndex);
}

double calibratedVoltage(double rawVolts)
{
    return rawVolts;
}

CalibratedAnalogInputs calibrate(const AnalogInputs &raw, const SupportedLimits &limits)
{
    double rating = limits.currentListFor(raw.appliedCurrentLimitType).at(raw.appliedCurrentLimitIndex);

    CalibratedAnalogInputs out;
    out.Ia = raw.Ia * rating;
    out.Ib = raw.Ib * rating;
    out.Ic = raw.Ic * rating;
    out.Ua = calibratedVoltage(raw.Ua);
    out.Ub = calibratedVoltage(raw.Ub);
    out.Uc = calibratedVoltage(raw.Uc);
    out.frequency = raw.frequency;
    out.phiIaUa = raw.phiIaUa;
    out.phiIbUb = raw.phiIbUb;
    out.phiIcUc = raw.phiIcUc;
    return out;
}

} // namespace avm_emon
