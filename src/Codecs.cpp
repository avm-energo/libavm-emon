#include "Codecs.h"

#include <utility>

namespace avm_emon::detail {

std::uint64_t wordsToUint(const QByteArray &words)
{
    std::uint64_t value = 0;
    for (int i = 0; i + 1 < words.size(); i += 2) {
        auto lo = static_cast<std::uint8_t>(words[i]);
        auto hi = static_cast<std::uint8_t>(words[i + 1]);
        auto word = static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
        value |= static_cast<std::uint64_t>(word) << (8 * i);
    }
    return value;
}

double decodeUFixed(const QByteArray &words, int fracBits)
{
    return static_cast<double>(wordsToUint(words)) / static_cast<double>(std::uint64_t(1) << fracBits);
}

double decodeSignedFixed(const QByteArray &words, int fracBits, int totalBits)
{
    std::uint64_t raw = wordsToUint(words);
    std::uint64_t signMask = std::uint64_t(1) << (totalBits - 1);
    std::uint64_t fullMask = (std::uint64_t(1) << totalBits) - 1;
    if (raw & signMask) {
        std::uint64_t magnitude = (~raw + 1) & fullMask; // "X = 0 - X" per the docs, full width
        return -(static_cast<double>(magnitude) / static_cast<double>(std::uint64_t(1) << fracBits));
    }
    return static_cast<double>(raw) / static_cast<double>(std::uint64_t(1) << fracBits);
}

QByteArray swapWordBytes(const QByteArray &raw)
{
    QByteArray out = raw;
    for (int i = 0; i + 1 < out.size(); i += 2)
        std::swap(out[i], out[i + 1]);
    return out;
}

QString decodeAscii(const QByteArray &raw)
{
    QByteArray fixed = raw;
    for (int i = 0; i < fixed.size(); ++i) {
        auto b = static_cast<std::uint8_t>(fixed[i]);
        if (b > 0xA0)
            fixed[i] = static_cast<char>((b + 0x20) & 0xFF);
    }
    QString s = QString::fromLatin1(fixed);
    qsizetype end = s.size();
    while (end > 0 && (s[end - 1] == QChar(0) || s[end - 1] == QChar(' ')))
        --end;
    return s.left(end);
}

double decodePower48(const QByteArray &words8)
{
    std::uint16_t signWord = static_cast<std::uint8_t>(words8[0])
                            | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(words8[1])) << 8);

    // Reassemble the 48-bit magnitude from words[1..3], which are
    // transmitted most-significant-word-first (see header comment) -
    // i.e. the opposite order wordsToUint() assumes, so reverse them
    // before delegating to it.
    QByteArray reordered;
    reordered.append(words8.mid(6, 2));
    reordered.append(words8.mid(4, 2));
    reordered.append(words8.mid(2, 2));
    std::uint64_t magnitude = wordsToUint(reordered);

    double value = static_cast<double>(magnitude) / static_cast<double>(std::uint64_t(1) << 20);
    return signWord == 0xFFFF ? -value : value;
}

} // namespace avm_emon::detail
