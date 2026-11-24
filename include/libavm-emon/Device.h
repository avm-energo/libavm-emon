#pragma once

#include "emon_export.h"
#include "Types.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <cstdint>
#include <memory>

namespace avm_emon::detail {
class Transport;
}

namespace avm_emon {

// EMon protocol device handle. Implements the functions this library
// targets: getDeviceInfo(), setLimits(), getSupportedLimits(),
// getAnalogInputs() - see md/plan.md for why the surface is
// intentionally this narrow.
//
// Not copyable (owns a serial port handle). Not thread-safe - use one
// Device per thread, or serialize access yourself.
class AVM_EMON_EXPORT Device {
public:
    // timeoutMs applies to every read; getAnalogInputs() (Mode 3) can
    // take several seconds and occasionally much longer (see
    // md/mindflow.md "Step 4.6") - the default here already accounts for
    // that, but pass a larger value if you see spurious ProtocolErrors.
    explicit Device(QString portName, LinkType linkType = LinkType::Rs232, int timeoutMs = 6000);
    ~Device();

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    void connectDevice();
    void disconnectDevice();
    bool isConnected() const;

    // Mode 15 (Read FRAM), reads the 512-byte system area.
    DeviceInfo getDeviceInfo();

    // Mode 2 (Set Limits), EM-3.1KM variant. Returns what the device
    // echoed back as actually applied.
    AppliedLimits setLimits(std::uint16_t currentLimitType,
                             std::uint16_t currentLimitIndex,
                             std::uint16_t voltageLimitIndex);

    // Mode 15 (Read FRAM). Decodes the variable-length supported-limits
    // table (system area offset 44 onward) - see md/mindflow.md "Step 9".
    // Needed alongside AnalogInputs::appliedCurrentLimit*/
    // appliedVoltageLimitIndex to show the active range.
    SupportedLimits getSupportedLimits();

    // Mode 3 (full calc, RMS + angle + frequency).
    AnalogInputs getAnalogInputs();

    // Diagnostics: the undecoded Mode 3 response payload (4536 bytes), for
    // reverse-engineering fields the library does not decode yet.
    QByteArray getAnalogInputsRaw();

private:
    QString m_portName;
    LinkType m_linkType;
    int m_timeoutMs;
    std::unique_ptr<detail::Transport> m_transport;
};

} // namespace avm_emon
