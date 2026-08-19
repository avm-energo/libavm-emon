"""CRC16 candidates for the EMon protocol.

CONFIRMED against a real capture (md/mindflow.md "Step 3.2/3.3 — CRC and
frame byte order confirmed"): CRC-16/XMODEM (poly=0x1021, init=0x0000, no
reflection, no xorout), computed over [SYNC][status/mode][len_lo][len_hi]
[payload...] - matched all 29 frames from 2 different response shapes with
distinct payload content each, so this is not a coincidental match.

Unlike every other multi-byte field in this protocol (little-endian), the
2-byte CRC itself is transmitted **big-endian** (high byte first) - see
`frame.py`'s `_pack_crc`/`_unpack_crc`.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List


def _reverse_byte(b: int) -> int:
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4)
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2)
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1)
    return b


def _reverse16(v: int) -> int:
    r = 0
    for _ in range(16):
        r = (r << 1) | (v & 1)
        v >>= 1
    return r


def _crc16_generic(data: bytes, poly: int, init: int, refin: bool, refout: bool, xorout: int) -> int:
    crc = init
    for byte in data:
        b = _reverse_byte(byte) if refin else byte
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ poly) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    if refout:
        crc = _reverse16(crc)
    return crc ^ xorout


@dataclass(frozen=True)
class Crc16Variant:
    name: str
    poly: int
    init: int
    refin: bool
    refout: bool
    xorout: int

    def compute(self, data: bytes) -> int:
        return _crc16_generic(data, self.poly, self.init, self.refin, self.refout, self.xorout)


# Candidates, roughly ordered by likelihood for this device class.
_VARIANT_LIST: List[Crc16Variant] = [
    Crc16Variant("MODBUS", 0x8005, 0xFFFF, True, True, 0x0000),
    Crc16Variant("CCITT-FALSE", 0x1021, 0xFFFF, False, False, 0x0000),
    Crc16Variant("XMODEM", 0x1021, 0x0000, False, False, 0x0000),
    Crc16Variant("KERMIT", 0x1021, 0x0000, True, True, 0x0000),
    Crc16Variant("CCITT-1D0F", 0x1021, 0x1D0F, False, False, 0x0000),
    Crc16Variant("ARC", 0x8005, 0x0000, True, True, 0x0000),
]
VARIANTS: Dict[str, Crc16Variant] = {v.name: v for v in _VARIANT_LIST}

# CONFIRMED against real hardware capture, see module docstring.
DEFAULT_VARIANT = VARIANTS["XMODEM"]


def compute(data: bytes, variant: Crc16Variant = DEFAULT_VARIANT) -> int:
    return variant.compute(data)


def guess_crc16(frame_without_crc: bytes, expected_crc_le: bytes) -> List[str]:
    """Return the names of all candidate variants whose CRC matches
    `expected_crc_le` (2 bytes, little-endian, as seen on the wire) for the
    given frame prefix (everything before the CRC field)."""
    expected = expected_crc_le[0] | (expected_crc_le[1] << 8)
    return [name for name, v in VARIANTS.items() if v.compute(frame_without_crc) == expected]
