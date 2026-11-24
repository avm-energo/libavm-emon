#include <libavm-emon/Calibration.h>

namespace avm_emon {

// The configured ranges are real measurement ranges of the input channels
// (device manual 4.2.3), not a transformer ratio, so the RMS values the
// device reports are already in amperes/volts. See md/mindflow.md "Step 11".

double calibratedCurrent(double rawAmps, const SupportedLimits &, std::uint16_t, std::uint16_t)
{
    return rawAmps;
}

double calibratedVoltage(double rawVolts)
{
    return rawVolts;
}

CalibratedAnalogInputs calibrate(const AnalogInputs &raw, const SupportedLimits &)
{
    CalibratedAnalogInputs out;
    out.Ia = raw.Ia;
    out.Ib = raw.Ib;
    out.Ic = raw.Ic;
    out.Ua = raw.Ua;
    out.Ub = raw.Ub;
    out.Uc = raw.Uc;
    out.frequency = raw.frequency;
    out.phiIaUa = raw.phiIaUa;
    out.phiIbUb = raw.phiIbUb;
    out.phiIcUc = raw.phiIcUc;
    out.phiUaUb = raw.phiUaUb;
    out.phiUbUc = raw.phiUbUc;
    out.phiUcUa = raw.phiUcUa;
    return out;
}

} // namespace avm_emon
