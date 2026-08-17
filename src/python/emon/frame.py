"""Low-level frame encode/decode for the EMon binary protocol.

Frame layout (see md/pseudocode.md "Frame format"), all multi-byte fields
are little-endian:

    request:  [SYNC=0x02][mode][len_lo][len_hi][payload...][crc_lo][crc_hi]
    response: [status][SYNC=0x02][len_lo][len_hi][payload...][crc_lo][crc_hi]

`status` is 0x00 for OK ("NORMA") or 0xFF for error ("NE NORMA"). CRC16
covers every byte before the CRC field itself.
"""
from __future__ import annotations

from dataclasses import dataclass

from . import crc

SYNC = 0x02
STATUS_OK = 0x00
STATUS_ERROR = 0xFF


class EMonProtocolError(Exception):
    """Frame is malformed, or doesn't match our understanding of the wire
    format (bad sync byte, bad CRC, truncated data, ...)."""


class EMonDeviceError(Exception):
    """Frame was well-formed but the device reported NOT-NORMA (0xFF)."""


def build_request(mode: int, payload: bytes = b"", crc_variant: crc.Crc16Variant = crc.DEFAULT_VARIANT) -> bytes:
    header = bytes([SYNC, mode]) + len(payload).to_bytes(2, "little")
    body = header + payload
    check = crc.compute(body, crc_variant).to_bytes(2, "little")
    return body + check


@dataclass
class Response:
    ok: bool
    payload: bytes


def parse_response(raw: bytes, crc_variant: crc.Crc16Variant = crc.DEFAULT_VARIANT) -> Response:
    if len(raw) < 6:
        raise EMonProtocolError(f"frame too short: {len(raw)} bytes (min 6)")

    status, sync = raw[0], raw[1]
    if sync != SYNC:
        raise EMonProtocolError(f"bad sync byte: {sync:#04x}, expected {SYNC:#04x}")

    length = int.from_bytes(raw[2:4], "little")
    body = raw[: 4 + length]
    if len(raw) < len(body) + 2:
        raise EMonProtocolError(
            f"frame shorter than declared length: have {len(raw)} bytes, "
            f"need {len(body) + 2} (length field says {length} payload bytes)"
        )

    payload = raw[4 : 4 + length]
    received_crc = int.from_bytes(raw[4 + length : 6 + length], "little")
    computed_crc = crc.compute(body, crc_variant)
    if received_crc != computed_crc:
        raise EMonProtocolError(
            f"CRC mismatch: frame says {received_crc:#06x}, computed "
            f"{computed_crc:#06x} with variant={crc_variant.name!r}. "
            f"Wrong CRC variant? see scripts/00_find_crc.py"
        )

    if status == STATUS_OK:
        return Response(True, payload)
    if status == STATUS_ERROR:
        return Response(False, payload)
    raise EMonProtocolError(f"unexpected status byte: {status:#04x} (expected 0x00 or 0xff)")
