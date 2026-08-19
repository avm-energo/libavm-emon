#pragma once

// Protocol constants: mode numbers and the Modes 3/4/5 payload layout.
// See md/pseudocode.md for the full derivation and offset tables, and
// md/mindflow.md "Step 4.3"/"Step 4.4"/"Step 4.5" for the corrections
// found against real hardware that pseudocode.md's original table got
// wrong.

#include <cstdint>

namespace avm_emon::detail {

enum class Mode : std::uint8_t {
    SetLimits = 0x02,
    CalcAll = 0x03,
    ReadFram = 0x0F,
};

constexpr std::uint8_t kSync = 0x02;
constexpr std::uint8_t kStatusOk = 0x00;
constexpr std::uint8_t kStatusError = 0xFF;

// Modes 3/4/5 all return a payload of this many bytes, laid out
// identically; the mode only controls which fields are actually computed.
constexpr int kWordsPerChannelBlock = 242;
constexpr int kChannelBlockCount = 6; // Ia, Ua, Ib, Ub, Ic, Uc
constexpr int kFooterWords = 64 + 2 + 150 + 600;
constexpr int kResponsePayloadBytes = (kWordsPerChannelBlock * kChannelBlockCount + kFooterWords) * 2; // 4536

// Word offset of field 5 (RMS phase value) inside a channel block.
// CORRECTED against real hardware: the doc-derived table said 204: see
// md/mindflow.md "Step 4.3".
constexpr int kField5RmsWordOffset = 205;

// Word offset of field 11 (angle u^i) inside a channel block. Only
// meaningful in the I blocks (Ia, Ib, Ic). CORRECTED against real
// hardware: the doc-derived table said 216, and the value is DEGREES,
// not radians: see md/mindflow.md "Step 4.4".
constexpr int kField11AngleWordOffset = 215;

// Word offset of field 33 (frequency) inside the footer. CORRECTED
// against real hardware: the doc-derived table said 40: see
// md/mindflow.md "Step 4.3".
constexpr int kField33FrequencyFooterWordOffset = 39;

constexpr int kFooterByteOffset = kWordsPerChannelBlock * kChannelBlockCount * 2;

// Channel index order matches the wire order: Ia, Ua, Ib, Ub, Ic, Uc.
enum ChannelIndex { ChIa = 0, ChUa = 1, ChIb = 2, ChUb = 3, ChIc = 4, ChUc = 5 };

inline int channelBlockByteOffset(int channelIndex)
{
    return channelIndex * kWordsPerChannelBlock * 2;
}

} // namespace avm_emon::detail
