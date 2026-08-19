#include "Frame.h"
#include "Crc.h"

#include <libavm-emon/Exceptions.h>

namespace avm_emon::detail {

QByteArray buildRequest(std::uint8_t mode, const QByteArray &payload)
{
    QByteArray header;
    header.append(static_cast<char>(kSync));
    header.append(static_cast<char>(mode));
    auto len = static_cast<std::uint16_t>(payload.size());
    header.append(static_cast<char>(len & 0xFF));
    header.append(static_cast<char>((len >> 8) & 0xFF));

    QByteArray body = header + payload;
    std::uint16_t crc = crc16Xmodem(body);
    // CRC is transmitted big-endian - the one field in this protocol
    // that isn't little-endian, see md/mindflow.md "Step 3.2/3.3".
    body.append(static_cast<char>((crc >> 8) & 0xFF));
    body.append(static_cast<char>(crc & 0xFF));
    return body;
}

Response parseResponse(const QByteArray &raw)
{
    if (raw.size() < 6)
        throw ProtocolError(QStringLiteral("frame too short: %1 bytes (min 6)").arg(raw.size()));

    auto sync = static_cast<std::uint8_t>(raw[0]);
    auto status = static_cast<std::uint8_t>(raw[1]);
    if (sync != kSync) {
        throw ProtocolError(QStringLiteral("bad sync byte: 0x%1, expected 0x%2")
                                 .arg(sync, 2, 16, QLatin1Char('0'))
                                 .arg(kSync, 2, 16, QLatin1Char('0')));
    }

    int length = static_cast<std::uint8_t>(raw[2]) | (static_cast<std::uint8_t>(raw[3]) << 8);
    int bodyLen = 4 + length;
    if (raw.size() < bodyLen + 2) {
        throw ProtocolError(QStringLiteral(
                                 "frame shorter than declared length: have %1 bytes, need %2 "
                                 "(length field says %3 payload bytes)")
                                 .arg(raw.size())
                                 .arg(bodyLen + 2)
                                 .arg(length));
    }

    QByteArray body = raw.left(bodyLen);
    QByteArray payload = raw.mid(4, length);
    auto receivedCrc = static_cast<std::uint16_t>((static_cast<std::uint8_t>(raw[bodyLen]) << 8)
                                                   | static_cast<std::uint8_t>(raw[bodyLen + 1]));
    std::uint16_t computedCrc = crc16Xmodem(body);
    if (receivedCrc != computedCrc) {
        throw ProtocolError(QStringLiteral("CRC mismatch: frame says 0x%1, computed 0x%2")
                                 .arg(receivedCrc, 4, 16, QLatin1Char('0'))
                                 .arg(computedCrc, 4, 16, QLatin1Char('0')));
    }

    if (status == kStatusOk)
        return Response{true, payload};
    if (status == kStatusError)
        return Response{false, payload};
    throw ProtocolError(QStringLiteral("unexpected status byte: 0x%1").arg(status, 2, 16, QLatin1Char('0')));
}

} // namespace avm_emon::detail
