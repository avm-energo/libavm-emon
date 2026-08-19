"""Decoders for the custom fixed-point / signed formats used in EMon
payloads. See md/pseudocode.md "Numeric formats" for the derivation and
the per-field frac_bits/total_bits table.
"""
from __future__ import annotations

from typing import Tuple


def words_to_uint(words: bytes) -> int:
    """Concatenate N little-endian 16-bit words (as read off the wire, in
    transmission order) into one unsigned integer, low word first."""
    value = 0
    for i in range(0, len(words), 2):
        word = int.from_bytes(words[i : i + 2], "little")
        value |= word << (8 * i)
    return value


def decode_ufixed(words: bytes, frac_bits: int) -> float:
    """Unsigned fixed point: integer part in the high bits, fraction in the
    low `frac_bits` bits of the concatenated value."""
    raw = words_to_uint(words)
    return raw / (1 << frac_bits)


def decode_signed_fixed(words: bytes, frac_bits: int, total_bits: int) -> float:
    """Signed fixed point (angles, mean value, K2u, ...), decoded per the
    doc's own rule: the MSB of the `total_bits`-wide value is the sign
    bit; when set, negate the *entire* value modulo 2**total_bits
    ("X = 0 - X", as the docs put it) to get the positive magnitude. This
    is standard two's-complement decoding - see md/mindflow.md "signed
    field decoding correction" for why an earlier sign-magnitude reading
    of this rule was wrong."""
    raw = words_to_uint(words)
    sign_mask = 1 << (total_bits - 1)
    full_mask = (1 << total_bits) - 1
    if raw & sign_mask:
        magnitude = (-raw) & full_mask
        sign = -1
    else:
        magnitude = raw
        sign = 1
    return sign * (magnitude / (1 << frac_bits))


def decode_power48(words: bytes) -> float:
    """4-word (8-byte) power field: word0 is a sign flag (0x0000=positive,
    0xFFFF=negative), words 1-3 are a 48-bit unsigned magnitude with 28
    integer bits and 20 fraction bits."""
    if len(words) != 8:
        raise ValueError(f"power field must be 4 words (8 bytes), got {len(words)}")
    sign_word = int.from_bytes(words[0:2], "little")
    magnitude = words_to_uint(words[2:8])
    sign = -1 if sign_word == 0xFFFF else 1
    return sign * (magnitude / (1 << 20))


def decode_ascii(raw: bytes) -> str:
    """ASCII text per the doc's quirky rule: bytes > 0xA0 need +0x20 (mod
    256) before being interpreted as a character code. Trailing NULs/spaces
    are stripped."""
    fixed = bytes(((b + 0x20) & 0xFF) if b > 0xA0 else b for b in raw)
    return fixed.decode("latin-1").rstrip("\x00 ")


def swap_word_bytes(raw: bytes) -> bytes:
    """Swap the byte order within each 16-bit word.

    CONFIRMED against a real capture (md/mindflow.md "Step 3.3"): ASCII
    text fields (e.g. the device-type string) are packed 2 characters per
    little-endian word, so each pair of characters arrives byte-swapped on
    the wire relative to reading order - e.g. the device-type string
    "EM3.1KM..." arrives as bytes `4d 45 2e 33 ...` ("ME.3K1..."), which
    swap_word_bytes + decode_ascii turns back into "EM3.1KM...". Call this
    before decode_ascii for any text field; raw is left unchanged if its
    length is odd (last byte kept as-is)."""
    out = bytearray(raw)
    for i in range(0, len(raw) - 1, 2):
        out[i], out[i + 1] = raw[i + 1], raw[i]
    return bytes(out)


def decode_daa_digit_string(word: int, digits: int) -> str:
    """A DAA-BCD word: each hex digit of the raw value IS a decimal digit.
    E.g. raw=0x2017 -> "2017"."""
    return f"{word:0{digits}x}"


def decode_daa_date(word_year: bytes, word_month_day: bytes) -> Tuple[int, int, int]:
    """2-word DAA-BCD date as used for the SW-version-date field, e.g.
    0x2017, 0x0113 -> (2017, 1, 13)."""
    year_word = int.from_bytes(word_year, "little")
    month_day_word = int.from_bytes(word_month_day, "little")
    year = int(decode_daa_digit_string(year_word, 4))
    month = int(decode_daa_digit_string((month_day_word >> 8) & 0xFF, 2))
    day = int(decode_daa_digit_string(month_day_word & 0xFF, 2))
    return year, month, day
