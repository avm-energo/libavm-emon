// Example: read live measurements (Mode 3 / full calc), with the retry
// pattern recommended for real applications.
//
// getAnalogInputs() already retries internally (Transport::request()'s
// default of 4 retries = 5 total attempts) before it would throw, so you
// never need to check byte counts or CRCs yourself - a successful return
// is already fully validated. The *outer* retry loop here is a second,
// independent layer on top of that: each call to getAnalogInputs() starts
// a fresh set of internal attempts, so retrying the whole call is a real
// extra chance, not wasted effort. This matters most over USB, where the
// large ~4.5KB payload has a real, confirmed intermittent stall
// characteristic (see md/mindflow.md "Step 6.1") - RS-232 essentially
// never needs the outer loop, USB benefits from it. Don't add delays
// between outer attempts: a failed attempt already costs real time
// (up to the configured timeout per internal try), so extra retries pile
// up worst-case latency fast - 2-3 is plenty.
//
// Usage:
//   get_analog_inputs <port> <rs232|usb>

#include <libavm-emon/Device.h>
#include <libavm-emon/Exceptions.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

namespace {
constexpr int kOuterAttempts = 3;
}

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

    // Mode 3 normally answers in ~2.5s; the default timeout here (6s)
    // gives comfortable margin for that without waiting too long on a
    // single stalled attempt - see the outer-retry rationale above.
    avm_emon::Device device(port, linkType, 6000);

    try {
        device.connectDevice();
    } catch (const avm_emon::Exception &e) {
        err << "connectDevice failed: " << e.message() << Qt::endl;
        return 1;
    }

    avm_emon::AnalogInputs values;
    bool ok = false;
    QString lastError;
    for (int attempt = 0; attempt < kOuterAttempts && !ok; ++attempt) {
        try {
            values = device.getAnalogInputs();
            ok = true;
        } catch (const avm_emon::Exception &e) {
            lastError = e.message();
        }
    }

    device.disconnectDevice();

    if (!ok) {
        err << "getAnalogInputs failed after " << kOuterAttempts << " attempts: " << lastError << Qt::endl;
        return 1;
    }

    // Ia/Ib/Ic/Ua/Ub/Uc are the device's raw pre-calibration reading, not
    // the calibrated value its own front panel shows - see
    // Device::getAnalogInputs()'s docstring. phi values are in degrees.
    out << QString("Ia = %1 A   Ua = %2 V   phi(Ia/Ua) = %3 deg")
               .arg(values.Ia, 0, 'f', 4).arg(values.Ua, 0, 'f', 4).arg(values.phiIaUa, 0, 'f', 4)
        << Qt::endl;
    out << QString("Ib = %1 A   Ub = %2 V   phi(Ib/Ub) = %3 deg")
               .arg(values.Ib, 0, 'f', 4).arg(values.Ub, 0, 'f', 4).arg(values.phiIbUb, 0, 'f', 4)
        << Qt::endl;
    out << QString("Ic = %1 A   Uc = %2 V   phi(Ic/Uc) = %3 deg")
               .arg(values.Ic, 0, 'f', 4).arg(values.Uc, 0, 'f', 4).arg(values.phiIcUc, 0, 'f', 4)
        << Qt::endl;
    out << QString("frequency = %1 Hz").arg(values.frequency, 0, 'f', 4) << Qt::endl;

    return 0;
}
