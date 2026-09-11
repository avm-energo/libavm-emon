#pragma once

// Turns AnalogInputs' raw pre-calibration readings into values matching
// the device's own front-panel display - see md/mindflow.md "Step 9" for
// the derivation and how this was confirmed against real hardware.

#include "Types.h"
#include "emon_export.h"

namespace avm_emon {

// CONFIRMED against real hardware (md/mindflow.md "Step 9"): the raw
// reading is always the true secondary-side current, unaffected by which
// range is configured (verified directly - cycling the configured range
// while holding a real current steady left the raw reading unchanged).
// The front panel's calibrated value is simply
// raw * (configured_range_rating_A / 1A), "1A" being the assumed
// standard CT secondary convention. currentLimitType is Mode 2's own
// numbering (0=transformer/1=clamp/2=UPTT), matching setLimits().
AVM_EMON_EXPORT double calibratedCurrent(double rawAmps, const SupportedLimits &limits,
                                          std::uint16_t currentLimitType, std::uint16_t currentLimitIndex);

// CONFIRMED against real hardware (md/mindflow.md "Step 4.3"/"Step 9"):
// voltage needs no scaling at all - the raw reading already matches the
// front panel exactly, regardless of the configured voltage range (which
// appears to just select ADC gain/precision, not a ratio).
AVM_EMON_EXPORT double calibratedVoltage(double rawVolts);

// Convenience: apply calibratedCurrent()/calibratedVoltage() to every
// channel of an AnalogInputs reading at once, using the limit
// configuration it was taken under (AnalogInputs::appliedCurrentLimit*/
// appliedVoltageLimitIndex).
AVM_EMON_EXPORT CalibratedAnalogInputs calibrate(const AnalogInputs &raw, const SupportedLimits &limits);

} // namespace avm_emon
