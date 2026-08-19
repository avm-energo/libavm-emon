"""Protocol constants: mode numbers and the Modes 3/4/5 payload layout.
See md/pseudocode.md for the full derivation and offset tables.
"""


class Mode:
    SET_LIMITS = 0x02
    CALC_ALL = 0x03
    CALC_IUP = 0x04  # currents/voltages/powers only, drives freq output
    CALC_HARMONICS = 0x05  # harmonics + angles + frequency etc.
    WRITE_RAM = 0x08
    READ_RAM = 0x09
    READ_NAND_FLASH = 0x0B
    READ_FRAM = 0x0F
    TOGGLE_FREQ_OUTPUT = 0x12
    SET_COEFFICIENTS = 0x13  # Mode 19 - body missing from the vendor docs, unimplemented
    CALC_FREQ_BASE = 0x14  # Mode 20
    CALC_IUP_SYNCED = 0x18  # Mode 24
    CALC_HARMONICS_SYNCED = 0x19  # Mode 25


STATUS_OK = 0x00
STATUS_ERROR = 0xFF

# Modes 3/4/5 all return a payload of this many bytes, laid out
# identically; the mode only controls which fields are actually computed.
WORDS_PER_CHANNEL_BLOCK = 242
CHANNEL_BLOCKS = ("Ia", "Ua", "Ib", "Ub", "Ic", "Uc")  # transmission order
FOOTER_WORDS = 64 + 2 + 150 + 600
RESPONSE_PAYLOAD_BYTES = (WORDS_PER_CHANNEL_BLOCK * len(CHANNEL_BLOCKS) + FOOTER_WORDS) * 2  # 4536

# Word offset of field 5 ("действующее фазное" / RMS phase value) inside a
# channel block. Meaningful in all 6 blocks.
#
# CORRECTED against real hardware (md/mindflow.md "Step 4.3"): the doc's
# field-width bookkeeping put this at 204, but a live capture with a known
# 10V/0.1A AC reference on phase A showed the real device data one word
# later - Ua at offset 205 decodes (frac_bits=20) to 10.0247, matching the
# meter's own front-panel reading to all 4 decimal digits; Ia at the same
# offset decodes to 0.0998, matching the ~0.1A actually fed in (this is
# the RAW pre-calibration value - the front panel's 0.998A applies the
# device's own internal current-ratio setting on top, which this protocol
# field does not do). Some field in 1-4 is undercounted by one word in the
# original doc-derived table; fields 6+ have not been individually
# reverified and may need the same +1 shift - see mindflow.md.
FIELD5_RMS_WORD_OFFSET = 205
# Word offset of field 11 ("угол u^i") inside a channel block. Only
# meaningful in the I blocks (Ia, Ib, Ic); reserved but unused in U blocks.
#
# UNRESOLVED (md/mindflow.md "Step 4.3"): given the field 5 correction
# above, this offset is now suspect too, but the same hardware test
# couldn't confirm or rule out a shift here - the test load was
# near-resistive (cos(phi) measured ~1.0 elsewhere in the payload), so the
# expected true angle is near zero either way, which doesn't discriminate
# between "correct offset, genuinely ~0" and "wrong offset, reading
# garbage that happens to look small". Needs a load with a known
# non-trivial phase angle (inductive/capacitive) to actually validate.
FIELD11_ANGLE_UI_WORD_OFFSET = 216
# Word offset of field 33 ("частота") inside the footer section.
#
# CORRECTED against real hardware (md/mindflow.md "Step 4.3"): scanning
# the footer of a live capture (50Hz nominal AC reference on phase A) for
# a plausible frequency found it one word *earlier* than documented, at
# footer word 39 (unsigned, frac_bits=16) -> 49.44 Hz - a very different,
# opposite-direction shift from field 5's, so likely an independent
# transcription error in the doc's footer field list, not the same root
# cause. ~1% off nominal 50Hz, plausible for real bench-source tolerance
# and far better than every neighboring offset (which read as raw
# 0xFFFF-ish flag/status words, not anything frequency-shaped).
FIELD33_FREQUENCY_FOOTER_WORD_OFFSET = 39

FOOTER_BYTE_OFFSET = WORDS_PER_CHANNEL_BLOCK * len(CHANNEL_BLOCKS) * 2


def channel_block_byte_offset(channel: str) -> int:
    return CHANNEL_BLOCKS.index(channel) * WORDS_PER_CHANNEL_BLOCK * 2
