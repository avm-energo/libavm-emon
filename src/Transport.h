#pragma once

#include <libavm-emon/Types.h>

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtSerialPort/QSerialPort>
#include <memory>

namespace avm_emon::detail {

// Serial transport for the EMon protocol. Owns and configures a
// QSerialPort for whichever LinkType it's given - see md/mindflow.md
// "Step 3.1" (RS-232 parameters) and "Step 5" (USB parameters), both
// confirmed against real hardware.
class Transport {
public:
    Transport(const QString &portName, LinkType linkType, int timeoutMs);
    ~Transport();

    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;

    // Send frame and read back exactly one response frame, using its
    // length field to know how many bytes to read.
    //
    // CONFIRMED against real hardware (md/mindflow.md "Step 4.6" and
    // "Step 6"): under back-to-back polling, response latency is
    // otherwise steady, but occasionally the device stalls mid-response -
    // roughly 1-in-15 to 25 requests over RS-232, much more often
    // (close to half, in testing) over USB at 3,000,000 baud. Once a
    // transfer stalls, waiting it out within the *same* attempt doesn't
    // reliably help - confirmed identical behaviour in the reference
    // Python/pyserial implementation, so this is a real device/link
    // characteristic, not a bug in this transport. Abandoning a stalled
    // attempt and retrying fresh (new write, new read) works far better
    // than a long single timeout, hence the higher default retry count
    // than a first guess would suggest. Pass retries=0 to disable and
    // fail fast.
    QByteArray request(const QByteArray &frame, int retries = 4);

private:
    QByteArray readExact(int n);

    std::unique_ptr<QSerialPort> m_port;
    int m_timeoutMs;
};

} // namespace avm_emon::detail
