#pragma once

#include <QtCore/QByteArray>
#include <cstdint>

namespace avm_emon::detail {

// CRC-16/XMODEM: poly=0x1021, init=0x0000, no reflection, no xorout.
// CONFIRMED against real hardware - see md/mindflow.md "Step 3.2/3.3".
// Note the CRC is transmitted big-endian on the wire (see Frame.cpp),
// unlike every other multi-byte field in this protocol.
std::uint16_t crc16Xmodem(const QByteArray &data);

} // namespace avm_emon::detail
