#include "Transport.h"

#include <libavm-emon/Exceptions.h>

#include <QtCore/QDeadlineTimer>

namespace avm_emon::detail {

Transport::Transport(const QString &portName, LinkType linkType, int timeoutMs)
    : m_port(std::make_unique<QSerialPort>())
    , m_timeoutMs(timeoutMs)
{
    m_port->setPortName(portName);
    switch (linkType) {
    case LinkType::Rs232:
        // CONFIRMED against real hardware, md/mindflow.md "Step 3.1".
        m_port->setBaudRate(115200);
        m_port->setStopBits(QSerialPort::OneStop);
        break;
    case LinkType::Usb:
        // CONFIRMED against real hardware, md/mindflow.md "Step 5".
        m_port->setBaudRate(3000000);
        m_port->setStopBits(QSerialPort::TwoStop);
        break;
    }
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        throw DeviceError(QStringLiteral("failed to open %1: %2").arg(portName, m_port->errorString()));
    }
}

Transport::~Transport() = default;

QByteArray Transport::readExact(int n)
{
    QByteArray data;
    QDeadlineTimer deadline(m_timeoutMs);
    while (data.size() < n) {
        qint64 remaining = deadline.remainingTime();
        if (remaining <= 0)
            break;
        if (m_port->bytesAvailable() <= 0) {
            if (!m_port->waitForReadyRead(static_cast<int>(remaining)))
                break;
        }
        data.append(m_port->read(n - data.size()));
    }
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
