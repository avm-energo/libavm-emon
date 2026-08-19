#include "Crc.h"

namespace avm_emon::detail {

std::uint16_t crc16Xmodem(const QByteArray &data)
{
    std::uint16_t crc = 0x0000;
    for (int i = 0; i < data.size(); ++i) {
        auto byte = static_cast<std::uint8_t>(data[i]);
        crc = static_cast<std::uint16_t>(crc ^ (static_cast<std::uint16_t>(byte) << 8));
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000)
                crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021);
            else
                crc = static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

} // namespace avm_emon::detail
