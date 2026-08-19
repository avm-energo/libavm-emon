// Minimal example: read the meter's identity (Mode 15 / Read FRAM).
//
// Usage:
//   get_device_info <port> <rs232|usb>
//
// Example:
//   get_device_info COM1 rs232
//   get_device_info COM11 usb

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
    if (args.size() != 3) {
        err << "Usage: " << argv[0] << " <port> <rs232|usb>" << Qt::endl;
        return 2;
    }

    const QString port = args[1];
    const avm_emon::LinkType linkType =
        args[2].toLower() == QLatin1String("usb") ? avm_emon::LinkType::Usb : avm_emon::LinkType::Rs232;

    // RS-232 and USB need different serial parameters under the hood
    // (115200 8N1 vs 3,000,000 8N2) - LinkType picks the pair that's
    // actually confirmed to work, see md/mindflow.md "Step 3.1"/"Step 5".
    avm_emon::Device device(port, linkType);

    try {
        device.connectDevice();
        const avm_emon::DeviceInfo info = device.getDeviceInfo();
        device.disconnectDevice();

        out << "device_type    = " << info.deviceType << Qt::endl;
        out << "serial_number  = " << info.serialNumber << Qt::endl;
        out << "device_type_id = " << info.deviceTypeId << Qt::endl;
    } catch (const avm_emon::Exception &e) {
        err << "getDeviceInfo failed: " << e.message() << Qt::endl;
        return 1;
    }

    return 0;
}
