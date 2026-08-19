// Manual hardware test for libavm-emon - mirrors py/scripts/02_test_get_info.py,
// 03_test_set_limits.py, and 04_test_get_analog_inputs.py. Not part of the
// installed package; dev-only convenience to validate the compiled library
// against real hardware (md/mindflow.md "Step 6").
//
// Usage:
//   manual_test <port> <rs232|usb> info
//   manual_test <port> <rs232|usb> analog
//   manual_test <port> <rs232|usb> limits <currentLimitType> <currentLimitIndex> <voltageLimitIndex>

#include <libavm-emon/Device.h>
#include <libavm-emon/Exceptions.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QTextStream>

#include <cstdlib>

namespace {

QTextStream out(stdout);
QTextStream err(stderr);

void printDeviceInfo(const avm_emon::DeviceInfo &info)
{
    out << "device_type       = " << info.deviceType << Qt::endl;
    out << "serial_number     = " << info.serialNumber << Qt::endl;
    out << "device_type_id    = " << info.deviceTypeId << Qt::endl;
    out << QString("sw_version_raw    = 0x%1, 0x%2")
               .arg(info.swVersionRawWord0, 4, 16, QLatin1Char('0'))
               .arg(info.swVersionRawWord1, 4, 16, QLatin1Char('0'))
        << Qt::endl;
    out << "  (SW version decoding is unresolved - see mindflow.md)" << Qt::endl;
}

void printAnalogInputs(const avm_emon::AnalogInputs &v)
{
    out << QString("Ia = %1 A   Ua = %2 V   phi(Ia/Ua) = %3 deg")
               .arg(v.Ia, 0, 'f', 4).arg(v.Ua, 0, 'f', 4).arg(v.phiIaUa, 0, 'f', 4)
        << Qt::endl;
    out << QString("Ib = %1 A   Ub = %2 V   phi(Ib/Ub) = %3 deg")
               .arg(v.Ib, 0, 'f', 4).arg(v.Ub, 0, 'f', 4).arg(v.phiIbUb, 0, 'f', 4)
        << Qt::endl;
    out << QString("Ic = %1 A   Uc = %2 V   phi(Ic/Uc) = %3 deg")
               .arg(v.Ic, 0, 'f', 4).arg(v.Uc, 0, 'f', 4).arg(v.phiIcUc, 0, 'f', 4)
        << Qt::endl;
    out << QString("frequency = %1 Hz").arg(v.frequency, 0, 'f', 4) << Qt::endl;
    out << Qt::endl;
    out << "RMS values (Ia/Ib/Ic/Ua/Ub/Uc) are the device's raw pre-calibration" << Qt::endl;
    out << "reading, not its own front-panel's calibrated value - see Device::getAnalogInputs()'s" << Qt::endl;
    out << "docstring." << Qt::endl;
}

void printAppliedLimits(const avm_emon::AppliedLimits &applied)
{
    out << "OK - device accepted the new limits (NORMA). Applied:" << Qt::endl;
    out << "  current_limit_type  = " << applied.currentLimitType << Qt::endl;
    out << "  current_limit_index = " << applied.currentLimitIndex << Qt::endl;
    out << "  voltage_limit_index = " << applied.voltageLimitIndex << Qt::endl;
    out << "  access_level        = " << applied.accessLevel << Qt::endl;
    out << QString("  wiring_scheme       = 0b%1").arg(applied.wiringScheme, 3, 2, QLatin1Char('0')) << Qt::endl;
}

int usage(const char *argv0)
{
    err << "Usage:" << Qt::endl;
    err << "  " << argv0 << " <port> <rs232|usb> info" << Qt::endl;
    err << "  " << argv0 << " <port> <rs232|usb> analog" << Qt::endl;
    err << "  " << argv0 << " <port> <rs232|usb> limits <currentLimitType> <currentLimitIndex> <voltageLimitIndex>"
        << Qt::endl;
    return 2;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();

    if (args.size() < 4)
        return usage(argv[0]);

    const QString port = args[1];
    const QString linkArg = args[2].toLower();
    const QString command = args[3].toLower();

    avm_emon::LinkType linkType;
    if (linkArg == QLatin1String("rs232")) {
        linkType = avm_emon::LinkType::Rs232;
    } else if (linkArg == QLatin1String("usb")) {
        linkType = avm_emon::LinkType::Usb;
    } else {
        err << "unknown link type: " << linkArg << " (expected rs232 or usb)" << Qt::endl;
        return usage(argv[0]);
    }

    // Mode 3 (analog inputs) normally takes ~2.5s; occasional stalls are
    // better handled by abandoning and retrying fresh than by waiting
    // one attempt out with a very long timeout - see md/mindflow.md
    // "Step 4.6"/"Step 6" and Transport::request()'s default retry count.
    avm_emon::Device device(port, linkType, 15000);

    try {
        device.connectDevice();

        if (command == QLatin1String("info")) {
            printDeviceInfo(device.getDeviceInfo());
        } else if (command == QLatin1String("analog")) {
            printAnalogInputs(device.getAnalogInputs());
        } else if (command == QLatin1String("limits")) {
            if (args.size() < 7) {
                device.disconnectDevice();
                return usage(argv[0]);
            }
            bool ok1 = false, ok2 = false, ok3 = false;
            auto currentLimitType = static_cast<std::uint16_t>(args[4].toUShort(&ok1));
            auto currentLimitIndex = static_cast<std::uint16_t>(args[5].toUShort(&ok2));
            auto voltageLimitIndex = static_cast<std::uint16_t>(args[6].toUShort(&ok3));
            if (!ok1 || !ok2 || !ok3) {
                err << "limit arguments must be non-negative integers" << Qt::endl;
                device.disconnectDevice();
                return 2;
            }
            printAppliedLimits(device.setLimits(currentLimitType, currentLimitIndex, voltageLimitIndex));
        } else {
            device.disconnectDevice();
            return usage(argv[0]);
        }

        device.disconnectDevice();
    } catch (const avm_emon::Exception &e) {
        err << "FAILED: " << e.message() << Qt::endl;
        return 1;
    }

    return 0;
}
