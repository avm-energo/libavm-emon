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
    // CONFIRMED against real hardware (md/mindflow.md "Step 4.6"): under
    // back-to-back polling, response latency is otherwise steady, but
    // occasionally (roughly 1-in-15 to 25 requests) the device stalls
    // for several seconds. A plain retry (fresh write, fresh read) has
    // recovered every time this was observed, so that's what `retries`
    // does - it copes with a real intermittent device hiccup, it isn't
    // masking a bug here. Pass retries=0 to disable and fail fast.
    QByteArray request(const QByteArray &frame, int retries = 1);

private:
    QByteArray readExact(int n);

    std::unique_ptr<QSerialPort> m_port;
    int m_timeoutMs;
};

} // namespace avm_emon::detail
