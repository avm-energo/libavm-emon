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
FIELD5_RMS_WORD_OFFSET = 204
# Word offset of field 11 ("угол u^i") inside a channel block. Only
# meaningful in the I blocks (Ia, Ib, Ic); reserved but unused in U blocks.
FIELD11_ANGLE_UI_WORD_OFFSET = 216
# Word offset of field 33 ("частота") inside the footer section.
FIELD33_FREQUENCY_FOOTER_WORD_OFFSET = 40

FOOTER_BYTE_OFFSET = WORDS_PER_CHANNEL_BLOCK * len(CHANNEL_BLOCKS) * 2


def channel_block_byte_offset(channel: str) -> int:
    return CHANNEL_BLOCKS.index(channel) * WORDS_PER_CHANNEL_BLOCK * 2
