#pragma once

#include "emon_export.h"

#include <QtCore/QString>
#include <QtCore/QVector>
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
    // DAA-digit decode of each raw word, e.g. raw (0x0702, 0x0153) ->
    // "702.153" - confirmed against the device's own display, which shows
    // this combined with the serial number as "702.153.519" (see
    // md/mindflow.md "Step 8"). The display separately shows a second,
    // shorter version ("2.01" on the unit tested) that is NOT decoded
    // from anywhere in this 512-byte area - likely a separate
    // bootloader/hardware version not exposed by this protocol at all.
    QString swVersion;
    std::uint16_t swVersionRawWord0 = 0;
    std::uint16_t swVersionRawWord1 = 0;
    // SW version date (year/month/day), same DAA-digit decode,
    // immediately following the version words - see md/mindflow.md "Step 8".
    int swVersionYear = 0, swVersionMonth = 0, swVersionDay = 0;
};

struct AVM_EMON_EXPORT AppliedLimits {
    std::uint16_t currentLimitType = 0;   // 0=transformer, 1=clamp, 2=UPTT
    std::uint16_t currentLimitIndex = 0;
    std::uint16_t voltageLimitIndex = 0;
    std::uint16_t accessLevel = 0;
    // 0b001 / 0b010 / 0b100 = 3-phase-4-wire / 3-phase-3-wire / 1-phase-2-wire
    std::uint16_t wiringScheme = 0;
};

// The device's supported current/voltage ranges, decoded from the system
// area's variable-length limits table - see md/mindflow.md "Step 9". Each
// list is index-aligned with the device's own indexing for that category
// (i.e. voltage[2] is what voltageLimitIndex=2 selects).
//
// voltage and currentTransformer are CONFIRMED against real hardware
// (19/19 exact matches against a real unit's own displayed limit lists).
// currentClamp/currentUptt use the same decode formula as
// currentTransformer (same physical quantity, same protocol) but that
// specific assumption is NOT independently confirmed - no clamp/UPTT
// hardware was available to check against.
struct AVM_EMON_EXPORT SupportedLimits {
    QVector<double> voltage;
    QVector<double> currentTransformer;
    QVector<double> currentClamp;
    QVector<double> currentUptt;

    // currentLimitType in Mode 2's own numbering (0=transformer, 1=clamp,
    // 2=UPTT), matching setLimits()'s parameter.
    const QVector<double> &currentListFor(std::uint16_t currentLimitType) const
    {
        switch (currentLimitType) {
        case 0: return currentTransformer;
        case 1: return currentClamp;
        default: return currentUptt;
        }
    }
};

// Ia/Ib/Ic/Ua/Ub/Uc are RMS values in amperes/volts exactly as the device
// reports them; they are NOT rescaled by the configured range (the ranges
// are real measurement ranges, see Calibration.h and md/mindflow.md
// "Step 11"). Calibration.h's calibrate() is kept for compatibility and
// returns these values unchanged.
struct AVM_EMON_EXPORT AnalogInputs {
    double Ia = 0, Ib = 0, Ic = 0;
    double Ua = 0, Ub = 0, Uc = 0;
    // Hz. Precise (about 1e-3 Hz): read from footer words 40-41, 16.16 with the
    // high word first, hardware-verified at 50 / 55 / 52.347 Hz, see
    // md/mindflow.md "Step 13". (Footer word 39, used in earlier versions,
    // is coarse and reads ~1.5% low.)
    double frequency = 0;
    // DEGREES, not radians - confirmed against real hardware, see
    // md/mindflow.md "Step 4.4".
    double phiIaUa = 0, phiIbUb = 0, phiIcUc = 0;
    // Angles between the first harmonics of the phase voltages, DEGREES
    // (the device's "Углы" screen): UA1-UB1, UB1-UC1, UC1-UA1. A positive
    // phiUaUb means UB lags UA by that angle (0/-120/-240 deg -> 120 each).
    // Hardware-verified, md/mindflow.md "Step 12".
    double phiUaUb = 0, phiUbUc = 0, phiUcUa = 0;
    // The limit configuration active when this reading was taken (fields
    // 45/46, footer word offsets 64/65 - CONFIRMED unshifted from the doc
    // against Mode 2's own echo, see md/mindflow.md "Step 4.2"). Needed
    // to know which range the reading was taken in (calibrate() does not need it) -
    // see md/mindflow.md "Step 9".
    std::uint16_t appliedCurrentLimitType = 0; // Mode 2 numbering: 0=transformer,1=clamp,2=UPTT
    std::uint16_t appliedCurrentLimitIndex = 0;
    std::uint16_t appliedVoltageLimitIndex = 0;
};

// Same values as AnalogInputs (kept for compatibility) - see calibrate().
struct AVM_EMON_EXPORT CalibratedAnalogInputs {
    double Ia = 0, Ib = 0, Ic = 0;
    double Ua = 0, Ub = 0, Uc = 0;
    double frequency = 0;
    double phiIaUa = 0, phiIbUb = 0, phiIcUc = 0;
    double phiUaUb = 0, phiUbUc = 0, phiUcUa = 0;
};

} // namespace avm_emon
