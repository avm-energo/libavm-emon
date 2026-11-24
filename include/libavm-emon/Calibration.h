#pragma once

// Historical API, kept for source compatibility. An earlier version scaled
// the current by the configured range ("CT ratio"); that was WRONG. The
// configured current/voltage ranges are real measurement ranges of the
// input channels (device manual, section 4.2.3: accuracy is specified from
// 0.2*In to 1.2*In of the selected range), so the RMS values in
// AnalogInputs are already in amperes/volts and match the front panel.
// Everything here is therefore an identity copy - see md/mindflow.md
// "Step 11" for the hardware evidence (5 A range, 0.299 A real current).

#include "Types.h"
#include "emon_export.h"

namespace avm_emon {

// Returns rawAmps unchanged (the limits arguments are ignored; they remain
// only so existing callers still compile). The selected current range is a
// measurement range, not a ratio.
AVM_EMON_EXPORT double calibratedCurrent(double rawAmps, const SupportedLimits &limits,
                                          std::uint16_t currentLimitType, std::uint16_t currentLimitIndex);

// Returns rawVolts unchanged.
AVM_EMON_EXPORT double calibratedVoltage(double rawVolts);

// Copies every channel of an AnalogInputs reading into a
// CalibratedAnalogInputs unchanged.
AVM_EMON_EXPORT CalibratedAnalogInputs calibrate(const AnalogInputs &raw, const SupportedLimits &limits);

} // namespace avm_emon
