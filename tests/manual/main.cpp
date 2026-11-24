// Manual hardware test for libavm-emon - mirrors py/scripts/02_test_get_info.py,
// 03_test_set_limits.py, and 04_test_get_analog_inputs.py. Not part of the
// installed package; dev-only convenience to validate the compiled library
// against real hardware (md/mindflow.md "Step 6").
//
// Usage:
//   manual_test <port> <rs232|usb> info
//   manual_test <port> <rs232|usb> analog
//   manual_test <port> <rs232|usb> limits <currentLimitType> <currentLimitIndex> <voltageLimitIndex>
//   manual_test <port> <rs232|usb> supported
//   manual_test <port> <rs232|usb> dump <file>   (raw Mode 3 payload)

#include <libavm-emon/Calibration.h>
#include <libavm-emon/Device.h>
#include <libavm-emon/Exceptions.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFile>
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
    out << QString("sw_version        = %1 (raw 0x%2, 0x%3)")
               .arg(info.swVersion)
               .arg(info.swVersionRawWord0, 4, 16, QLatin1Char('0'))
               .arg(info.swVersionRawWord1, 4, 16, QLatin1Char('0'))
        << Qt::endl;
    out << QString("sw_version_date   = %1-%2-%3")
               .arg(info.swVersionYear, 4, 10, QLatin1Char('0'))
               .arg(info.swVersionMonth, 2, 10, QLatin1Char('0'))
               .arg(info.swVersionDay, 2, 10, QLatin1Char('0'))
        << Qt::endl;
    out << "  (the device's own display may show a second, shorter version number -" << Qt::endl;
    out << "   that one isn't decoded from anywhere in this 512-byte area, see mindflow.md)" << Qt::endl;
}

void printAnalogInputs(const avm_emon::AnalogInputs &v, const avm_emon::SupportedLimits &limits)
{
    const avm_emon::CalibratedAnalogInputs cal = avm_emon::calibrate(v, limits);

    out << "measured:" << Qt::endl;
    out << QString("  Ia = %1 A   Ua = %2 V   phi(Ia/Ua) = %3 deg")
               .arg(v.Ia, 0, 'f', 4).arg(v.Ua, 0, 'f', 4).arg(v.phiIaUa, 0, 'f', 4)
        << Qt::endl;
    out << QString("  Ib = %1 A   Ub = %2 V   phi(Ib/Ub) = %3 deg")
               .arg(v.Ib, 0, 'f', 4).arg(v.Ub, 0, 'f', 4).arg(v.phiIbUb, 0, 'f', 4)
        << Qt::endl;
    out << QString("  Ic = %1 A   Uc = %2 V   phi(Ic/Uc) = %3 deg")
               .arg(v.Ic, 0, 'f', 4).arg(v.Uc, 0, 'f', 4).arg(v.phiIcUc, 0, 'f', 4)
        << Qt::endl;
    out << QString("  UA1-UB1 = %1 deg   UB1-UC1 = %2 deg   UC1-UA1 = %3 deg")
               .arg(v.phiUaUb, 0, 'f', 3).arg(v.phiUbUc, 0, 'f', 3).arg(v.phiUcUa, 0, 'f', 3) << Qt::endl;
    out << QString("  frequency = %1 Hz").arg(v.frequency, 0, 'f', 4) << Qt::endl;
    out << Qt::endl;
    out << "calibrate() (identity, kept for compatibility - see md/mindflow.md \"Step 11\"):" << Qt::endl;
    out << QString("  Ia = %1 A   Ua = %2 V").arg(cal.Ia, 0, 'f', 4).arg(cal.Ua, 0, 'f', 4) << Qt::endl;
    out << QString("  Ib = %1 A   Ub = %2 V").arg(cal.Ib, 0, 'f', 4).arg(cal.Ub, 0, 'f', 4) << Qt::endl;
    out << QString("  Ic = %1 A   Uc = %2 V").arg(cal.Ic, 0, 'f', 4).arg(cal.Uc, 0, 'f', 4) << Qt::endl;
    out << Qt::endl;
    out << QString("  (active current range: type=%1 index=%2, voltage range index=%3)")
               .arg(v.appliedCurrentLimitType).arg(v.appliedCurrentLimitIndex).arg(v.appliedVoltageLimitIndex)
        << Qt::endl;
}

void printSupportedLimits(const avm_emon::SupportedLimits &limits)
{
    auto printList = [](const char *label, const QVector<double> &values) {
        QStringList parts;
        for (double v : values)
            parts << QString::number(v, 'g', 6);
        out << label << ": " << parts.join(", ") << Qt::endl;
    };
    printList("voltage             ", limits.voltage);
    printList("current_transformer ", limits.currentTransformer);
    printList("current_clamp       ", limits.currentClamp);
    printList("current_uptt        ", limits.currentUptt);
    out << Qt::endl;
    out << "voltage/current_transformer confirmed against real hardware (md/mindflow.md" << Qt::endl;
    out << "\"Step 9\"); current_clamp/current_uptt use the same decode formula but are NOT" << Qt::endl;
    out << "independently confirmed - no clamp/UPTT hardware was available to check." << Qt::endl;
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
    err << "  " << argv0 << " <port> <rs232|usb> supported" << Qt::endl;
    err << "  " << argv0 << " <port> <rs232|usb> dump <file>" << Qt::endl;
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
            avm_emon::AnalogInputs values = device.getAnalogInputs();
            avm_emon::SupportedLimits limits = device.getSupportedLimits();
            printAnalogInputs(values, limits);
        } else if (command == QLatin1String("dump")) {
            // Raw Mode 3 payload to a file, for offline field hunting (e.g. field 12, angle u^u).
            if (args.size() < 5) {
                device.disconnectDevice();
                return usage(argv[0]);
            }
            const QByteArray raw = device.getAnalogInputsRaw();
            QFile f(args[4]);
            if (!f.open(QIODevice::WriteOnly) || f.write(raw) != raw.size()) {
                err << "cannot write " << args[4] << Qt::endl;
                device.disconnectDevice();
                return 1;
            }
            out << "wrote " << raw.size() << " bytes to " << args[4] << Qt::endl;
        } else if (command == QLatin1String("supported")) {
            printSupportedLimits(device.getSupportedLimits());
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
