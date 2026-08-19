#pragma once

#include "emon_export.h"

#include <QtCore/QString>
#include <cstdint>

namespace avm_emon {

// Which physical link a Device is connected through. Determines the
// serial parameters used - see md/mindflow.md "Step 3.1" (RS-232) and
// "Step 5" (USB) for how these were confirmed against real hardware.
// There is no generic "custom baud/stop-bits" option on purpose: those
// are the only two combinations actually verified to work.
enum class LinkType {
    Rs232, // 115200 baud, 8N1
    Usb,   // 3,000,000 baud, 8N2 (via the device's FTDI-chip USB port)
};

struct AVM_EMON_EXPORT DeviceInfo {
    QString deviceType;
    std::uint16_t serialNumber = 0;
    std::uint16_t deviceTypeId = 0;
    // SW version encoding is unresolved - see md/mindflow.md "SW version
    // field is unresolved". Exposed as raw words until confirmed.
    std::uint16_t swVersionRawWord0 = 0;
    std::uint16_t swVersionRawWord1 = 0;
};

struct AVM_EMON_EXPORT AppliedLimits {
    std::uint16_t currentLimitType = 0;   // 0=transformer, 1=clamp, 2=UPTT
    std::uint16_t currentLimitIndex = 0;
    std::uint16_t voltageLimitIndex = 0;
    std::uint16_t accessLevel = 0;
    // 0b001 / 0b010 / 0b100 = 3-phase-4-wire / 3-phase-3-wire / 1-phase-2-wire
    std::uint16_t wiringScheme = 0;
};

// NOTE (md/mindflow.md "Step 4.3"): Ia/Ib/Ic/Ua/Ub/Uc are the device's raw
// pre-calibration RMS readings, not the calibrated value its own front
// panel displays - the panel additionally applies the configured
// current-transformer ratio (see AppliedLimits::currentLimitType/Index),
// which this protocol field does not.
struct AVM_EMON_EXPORT AnalogInputs {
    double Ia = 0, Ib = 0, Ic = 0;
    double Ua = 0, Ub = 0, Uc = 0;
    double frequency = 0;
    // DEGREES, not radians - confirmed against real hardware, see
    // md/mindflow.md "Step 4.4".
    double phiIaUa = 0, phiIbUb = 0, phiIcUc = 0;
};

} // namespace avm_emon
