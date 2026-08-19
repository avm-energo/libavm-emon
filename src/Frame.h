#pragma once

// Frame layout, CONFIRMED against real hardware (md/mindflow.md "Step
// 3.2/3.3"). All multi-byte fields are little-endian **except the CRC**,
// which is transmitted big-endian (high byte first):
//
//   request:  [SYNC=0x02][mode]  [len_lo][len_hi][payload...][crc_hi][crc_lo]
//   response: [SYNC=0x02][status][len_lo][len_hi][payload...][crc_hi][crc_lo]
//
// Both directions put SYNC first. status is 0x00 for OK ("NORMA") or
// 0xFF for error ("NE NORMA"). CRC16 covers every byte before the CRC
// field itself.

#include "Constants.h"

#include <QtCore/QByteArray>
#include <cstdint>

namespace avm_emon::detail {

struct Response {
    bool ok = false;
    QByteArray payload;
};

QByteArray buildRequest(std::uint8_t mode, const QByteArray &payload = {});

// Throws avm_emon::ProtocolError if raw isn't a well-formed frame (bad
// sync byte, bad CRC, truncated data, unexpected status byte).
Response parseResponse(const QByteArray &raw);

} // namespace avm_emon::detail
