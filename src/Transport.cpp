#include "Transport.h"

#include <libavm-emon/Exceptions.h>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

namespace avm_emon::detail {

Transport::Transport(const QString &portName, LinkType linkType, int timeoutMs)
    : m_port(std::make_unique<QSerialPort>())
    , m_timeoutMs(timeoutMs)
{
    // Open FIRST, then configure - matching Qt's own examples. Setting
    // parameters before open() is documented to work, but the Windows
    // backend doesn't reliably apply pre-open settings in practice.
    m_port->setPortName(portName);
    if (!m_port->open(QIODevice::ReadWrite)) {
        throw DeviceError(QStringLiteral("failed to open %1: %2").arg(portName, m_port->errorString()));
    }

    bool ok = true;
    switch (linkType) {
    case LinkType::Rs232:
        // CONFIRMED against real hardware, md/mindflow.md "Step 3.1".
        ok = ok && m_port->setBaudRate(115200);
        ok = ok && m_port->setStopBits(QSerialPort::OneStop);
        break;
    case LinkType::Usb:
        // CONFIRMED against real hardware, md/mindflow.md "Step 5".
        ok = ok && m_port->setBaudRate(3000000);
        ok = ok && m_port->setStopBits(QSerialPort::TwoStop);
        break;
    }
    ok = ok && m_port->setDataBits(QSerialPort::Data8);
    ok = ok && m_port->setParity(QSerialPort::NoParity);
    ok = ok && m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!ok) {
        QString error = m_port->errorString();
        m_port->close();
        throw DeviceError(QStringLiteral("failed to configure %1: %2").arg(portName, error));
    }

    // CONFIRMED against real hardware (md/mindflow.md "Step 6"): unlike
    // pyserial (used throughout this project's Python implementation and
    // its hardware validation), QSerialPort does not assert DTR/RTS by
    // default. Without this, the device produced total silence - no
    // error, just zero bytes back, indistinguishable from a wrong
    // baud/framing guess until traced down to this. Whether the device
    // genuinely needs these lines electrically (e.g. opto-isolator power,
    // matching a historical theory from Step 3.1's RS-232 bring-up) or
    // some intermediate adapter does, asserting both unconditionally
    // matches pyserial's effective behavior and is what's confirmed
    // working.
    m_port->setDataTerminalReady(true);
    m_port->setRequestToSend(true);
}

Transport::~Transport() = default;

QByteArray Transport::readExact(int n)
{
    // Event-loop-driven (readyRead signal + QEventLoop) rather than the
    // blocking waitForReadyRead()/read() polling pattern - the pattern
    // Qt's own serial port examples use for bulk reads. Kept as good
    // practice, but NOTE (md/mindflow.md "Step 6"): this did not fix the
    // large-transfer short-read symptom seen at USB's 3,000,000 baud:
    // that turned out to be a genuine intermittent link/device
    // characteristic specific to large (4500+ byte) transfers over USB,
    // confirmed identical in the reference Python/pyserial
    // implementation - not a bug in how this function reads. Mitigated
    // via Transport::request()'s retry count, not by anything in here.
    QByteArray data;
    if (m_port->bytesAvailable() > 0) {
        data.append(m_port->read(n - data.size()));
        if (data.size() >= n)
            return data;
    }

    QEventLoop loop;
    QMetaObject::Connection conn = QObject::connect(m_port.get(), &QSerialPort::readyRead, &loop, [&]() {
        data.append(m_port->read(n - data.size()));
        if (data.size() >= n)
            loop.quit();
    });
    QTimer::singleShot(m_timeoutMs, &loop, &QEventLoop::quit);

    loop.exec();

    QObject::disconnect(conn);
    return data;
}

QByteArray Transport::request(const QByteArray &frame, int retries)
{
    qsizetype lastPartialSize = 0;
    for (int attempt = 0; attempt <= retries; ++attempt) {
        m_port->clear(QSerialPort::Input);
        m_port->write(frame);
        if (!m_port->waitForBytesWritten(m_timeoutMs)) {
            if (attempt < retries)
                continue;
            throw ProtocolError(QStringLiteral("timed out writing request"));
        }

        QByteArray header = readExact(4);
        if (header.size() != 4) {
            lastPartialSize = header.size();
            if (attempt < retries)
                continue;
            throw ProtocolError(QStringLiteral("expected 4 header bytes, got %1").arg(header.size()));
        }

        int length = static_cast<std::uint8_t>(header[2]) | (static_cast<std::uint8_t>(header[3]) << 8);
        QByteArray rest = readExact(length + 2);
        if (rest.size() != length + 2) {
            lastPartialSize = header.size() + rest.size();
            if (attempt < retries)
                continue;
            throw ProtocolError(QStringLiteral("expected %1 bytes after header, got %2")
                                     .arg(length + 2)
                                     .arg(rest.size()));
        }

        return header + rest;
    }
    throw ProtocolError(
        QStringLiteral("request failed after retries, last partial read: %1 bytes").arg(lastPartialSize));
}

} // namespace avm_emon::detail
