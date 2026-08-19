#pragma once

// Decoders for the fixed-point / signed / ASCII formats used in EMon
// payloads. See md/pseudocode.md "Numeric formats" for the derivation.

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <cstdint>

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

} // namespace avm_emon::detail
