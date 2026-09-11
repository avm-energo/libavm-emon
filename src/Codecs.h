#pragma once

// Decoders for the fixed-point / signed / ASCII formats used in EMon
// payloads. See md/pseudocode.md "Numeric formats" for the derivation.

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <cstdint>
#include <utility>

namespace avm_emon::detail {

// Concatenate N little-endian 16-bit words (as read off the wire, in
// transmission order) into one unsigned integer, low word first.
std::uint64_t wordsToUint(const QByteArray &words);

// Unsigned fixed point: integer part in the high bits, fraction in the
// low fracBits bits of the concatenated value.
double decodeUFixed(const QByteArray &words, int fracBits);

// Signed fixed point (angles, mean value, ...), decoded per the docs'
// own rule: the MSB of the totalBits-wide value is the sign bit; when
// set, negate the *entire* value modulo 2**totalBits ("X = 0 - X") to
// get the positive magnitude - standard two's-complement, not
// sign-magnitude. See md/mindflow.md "signed field decoding correction".
double decodeSignedFixed(const QByteArray &words, int fracBits, int totalBits);

// Swap the byte order within each 16-bit word. CONFIRMED against real
// hardware (md/mindflow.md "Step 3.4"): ASCII text fields are packed 2
// characters per little-endian word, so each character pair arrives
// byte-swapped relative to reading order. Call this before decodeAscii
// for any text field.
QByteArray swapWordBytes(const QByteArray &raw);

// ASCII text per the doc's quirky rule: bytes > 0xA0 need +0x20 (mod
// 256) before being interpreted as a character code. Trailing NULs and
// spaces are stripped.
QString decodeAscii(const QByteArray &raw);

// 4-word (8-byte) power field: word0 is a sign flag (0x0000=positive,
// 0xFFFF=negative), words 1-3 are a 48-bit unsigned magnitude with 28
// integer bits and 20 fraction bits.
//
// CONFIRMED against real hardware (md/mindflow.md "Step 4.5"): unlike
// every other multi-word field in this protocol, the magnitude's 3 words
// are transmitted most-significant-word-first, not least-significant-
// first.
double decodePower48(const QByteArray &words8);

// DAA-BCD: each hex digit of the raw word IS a decimal digit, e.g.
// raw=0x0702 -> "0702". Used for the SW version and SW version date
// fields - CONFIRMED against the device's own display, see
// md/mindflow.md "Step 8".
QString decodeDaaDigitString(std::uint16_t word, int digits);

// 2-word DAA-BCD date (year word, month/day word), e.g.
// (0x2014, 0x0624) -> {2014, 6, 24}.
struct DaaDate {
    int year, month, day;
};
DaaDate decodeDaaDate(std::uint16_t yearWord, std::uint16_t monthDayWord);

// Variable-length supported-limits table (system area offset 44,
// spilling into the doc's mislabeled adjacent "RESERVE" region) - see
// md/mindflow.md "Step 9". Each entry is 3 words: type, int_word,
// frac_word; a type=0 entry terminates the list. `divisorByType` maps a
// raw type value to its divisor (voltage=64, current categories=512).
// Returns (type, value) pairs, value = int_word/D + frac_word/(D*65536).
QVector<std::pair<std::uint16_t, double>> decodeLimitsTable(const QByteArray &raw,
                                                             const QHash<std::uint16_t, int> &divisorByType);

} // namespace avm_emon::detail
