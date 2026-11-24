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

// Word offset of field 12 (angle between phase voltages, "u^u") inside a
// channel block. Defined only in the U blocks: Ua block = UA1-UB1, Ub block
// = UB1-UC1, Uc block = UC1-UA1 (degrees, signed 16.16). Found against real
// hardware with voltages at 0/-90/-200 deg: md/mindflow.md "Step 12".
constexpr int kField12AngleUuWordOffset = 217;

// Word offset of field 33 (frequency) inside the footer. CORRECTED
// against real hardware: the doc-derived table said 40: see
// md/mindflow.md "Step 4.3".
constexpr int kField33FrequencyFooterWordOffset = 39;

// Word offsets of fields 45/46 (applied current-limit code, applied
// voltage-limit index) inside the footer. UNSHIFTED from the doc-derived
// table - CONFIRMED against real hardware in md/mindflow.md "Step 4.2"
// (agreed with Mode 2's own echo), so field 33's -1 shift was a local
// transcription error, not something that propagates to every field
// after it. Field 45 packs two values into one word: high byte =
// current-limit type (0=transformer, 1=clamp, 2=UPTT - Mode 2's own
// numbering), low byte = current-limit index. Field 46 is a plain
// voltage-limit index.
constexpr int kField45CurrentLimitFooterWordOffset = 64;
constexpr int kField46VoltageLimitFooterWordOffset = 65;

constexpr int kFooterByteOffset = kWordsPerChannelBlock * kChannelBlockCount * 2;

// Byte offset of the supported current/voltage limits table within the
// 512-byte system area (Mode 15 FRAM read) - see md/mindflow.md "Step 9".
// Variable-length, terminated by a type=0 entry; each entry is 3 words
// (type, int_word, frac_word).
constexpr int kLimitsTableByteOffset = 44;

// The limits table's OWN type numbering - NOT the same as Mode 2's
// current-limit type (0=transformer/1=clamp/2=UPTT). Table type = Mode 2
// type + 2 for the current categories; voltage has no Mode 2 equivalent.
enum class LimitTableType : std::uint16_t {
    NoMoreLimits = 0,
    Voltage = 1,
    CurrentTransformer = 2,
    CurrentClamp = 3,
    CurrentUptt = 4,
};

// Channel index order matches the wire order: Ia, Ua, Ib, Ub, Ic, Uc.
enum ChannelIndex { ChIa = 0, ChUa = 1, ChIb = 2, ChUb = 3, ChIc = 4, ChUc = 5 };

inline int channelBlockByteOffset(int channelIndex)
{
    return channelIndex * kWordsPerChannelBlock * 2;
}

} // namespace avm_emon::detail
