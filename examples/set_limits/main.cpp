// Example: apply current/voltage limits (Mode 2, EM-3.1KM variant) and
// print back what the device actually applied.
//
// Note this WRITES device configuration - unlike the other two examples,
// running it changes the meter's state. Read the applied values back
// (printed below) if you need to restore a previous configuration.
//
// Usage:
//   set_limits <port> <rs232|usb> <currentLimitType> <currentLimitIndex> <voltageLimitIndex>
//
// currentLimitType: 0=transformer, 1=clamp, 2=UPTT

#include <libavm-emon/Device.h>
#include <libavm-emon/Exceptions.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() != 6) {
        err << "Usage: " << argv[0]
            << " <port> <rs232|usb> <currentLimitType> <currentLimitIndex> <voltageLimitIndex>" << Qt::endl;
        return 2;
    }

    const QString port = args[1];
    const avm_emon::LinkType linkType =
        args[2].toLower() == QLatin1String("usb") ? avm_emon::LinkType::Usb : avm_emon::LinkType::Rs232;

    bool ok1 = false, ok2 = false, ok3 = false;
    const auto currentLimitType = static_cast<std::uint16_t>(args[3].toUShort(&ok1));
    const auto currentLimitIndex = static_cast<std::uint16_t>(args[4].toUShort(&ok2));
    const auto voltageLimitIndex = static_cast<std::uint16_t>(args[5].toUShort(&ok3));
    if (!ok1 || !ok2 || !ok3) {
        err << "currentLimitType/currentLimitIndex/voltageLimitIndex must be non-negative integers" << Qt::endl;
        return 2;
    }

    avm_emon::Device device(port, linkType);

    try {
        device.connectDevice();
        const avm_emon::AppliedLimits applied = device.setLimits(currentLimitType, currentLimitIndex, voltageLimitIndex);
        device.disconnectDevice();

        out << "Device accepted the new limits (NORMA). Applied:" << Qt::endl;
        out << "  current_limit_type  = " << applied.currentLimitType << Qt::endl;
        out << "  current_limit_index = " << applied.currentLimitIndex << Qt::endl;
        out << "  voltage_limit_index = " << applied.voltageLimitIndex << Qt::endl;
        out << "  access_level        = " << applied.accessLevel << Qt::endl;
        out << QString("  wiring_scheme       = 0b%1").arg(applied.wiringScheme, 3, 2, QLatin1Char('0'))
            << Qt::endl;
    } catch (const avm_emon::Exception &e) {
        err << "setLimits failed: " << e.message() << Qt::endl;
        return 1;
    }

    return 0;
}
