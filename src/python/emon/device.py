"""High level EMon device API.

Implements the four functions targeted for the first milestone:
connect/disconnect, get_device_info, set_current_limits, get_analog_inputs.
Everything here is unverified against real hardware yet - see
md/plan.md step 3 and md/mindflow.md for the open questions.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Tuple

from . import codecs
from . import constants as c
from . import crc
from .frame import EMonDeviceError, EMonProtocolError, build_request, parse_response
from .transport import DEFAULT_BAUDRATE, DEFAULT_TIMEOUT_S, SerialTransport


@dataclass
class DeviceInfo:
    device_type: str
    serial_number: int
    # SW version encoding is unresolved - the two source docs disagree on
    # field width and the one worked example doesn't decode cleanly under
    # any of DAA/hex/decimal. Exposed as raw words until confirmed on real
    # hardware (see mindflow.md "SW version field is unresolved").
    sw_version_raw_words: Tuple[int, int]
    device_type_id: int


@dataclass
class AppliedLimits:
    current_limit_type: int
    current_limit_index: int
    voltage_limit_index: int
    access_level: int
    wiring_scheme: int  # 0b001/0b010/0b100 = 3-phase-4-wire/3-phase-3-wire/1-phase-2-wire


@dataclass
class AnalogInputs:
    Ia: float
    Ib: float
    Ic: float
    Ua: float
    Ub: float
    Uc: float
    frequency: float
    phi_ia_ua: float
    phi_ib_ub: float
    phi_ic_uc: float


class EMonDevice:
    def __init__(
        self,
        port: str,
        baudrate: int = DEFAULT_BAUDRATE,
        crc_variant: crc.Crc16Variant = crc.DEFAULT_VARIANT,
    ):
        self._port = port
        self._baudrate = baudrate
        self._crc_variant = crc_variant
        self._transport: Optional[SerialTransport] = None

    # -- connection ----------------------------------------------------
    def connect(self, timeout: float = DEFAULT_TIMEOUT_S) -> None:
        self._transport = SerialTransport(self._port, self._baudrate, timeout)

    def disconnect(self) -> None:
        if self._transport is not None:
            self._transport.close()
            self._transport = None

    def __enter__(self) -> "EMonDevice":
        self.connect()
        return self

    def __exit__(self, *exc) -> None:
        self.disconnect()

    def _call(self, mode: int, payload: bytes = b"") -> bytes:
        if self._transport is None:
            raise RuntimeError("not connected - call connect() first")
        request = build_request(mode, payload, self._crc_variant)
        raw = self._transport.request(request)
        response = parse_response(raw, self._crc_variant)
        if not response.ok:
            raise EMonDeviceError(f"device returned NOT-NORMA for mode {mode:#04x}")
        return response.payload

    # -- device info -----------------------------------------------------
    def get_device_info(self) -> DeviceInfo:
        """Read the 512-byte system area via Mode 15 (Read FRAM). The area
        ends with its own CRC at offset 510, matching Mode 15's "block
        closed by CRC" addressing - see mindflow.md step 1."""
        payload = self._read_fram(start_word=0, count_words=256)
        device_type = codecs.decode_ascii(codecs.swap_word_bytes(payload[0:16]))
        serial_number = int.from_bytes(payload[16:18], "little")
        device_type_id = int.from_bytes(payload[26:28], "little")
        sw_version_raw_words = (
            int.from_bytes(payload[18:20], "little"),
            int.from_bytes(payload[20:22], "little"),
        )
        return DeviceInfo(device_type, serial_number, sw_version_raw_words, device_type_id)

    def _read_fram(self, start_word: int, count_words: int) -> bytes:
        payload = start_word.to_bytes(2, "little") + count_words.to_bytes(2, "little")
        return self._call(c.Mode.READ_FRAM, payload)

    # -- limits ------------------------------------------------------------
    def set_current_limits(
        self, current_limit_type: int, current_limit_index: int, voltage_limit_index: int
    ) -> AppliedLimits:
        """Mode 2, EM-3.1KM variant (type + current index + voltage index) -
        this is the variant matching our device's own name in the vendor
        docs; the shorter EM-3.3T1/EM-3.1K variant (index + voltage index
        only) is not implemented. See mindflow.md. Returns what the device
        echoed back as actually applied (see pseudocode.md "Mode 2")."""
        payload = (
            current_limit_type.to_bytes(2, "little")
            + current_limit_index.to_bytes(2, "little")
            + voltage_limit_index.to_bytes(2, "little")
        )
        response = self._call(c.Mode.SET_LIMITS, payload)
        if len(response) != 10:
            raise EMonProtocolError(f"unexpected set_current_limits response size: {len(response)}, expected 10")
        words = [int.from_bytes(response[i : i + 2], "little") for i in range(0, 10, 2)]
        return AppliedLimits(
            current_limit_type=words[0],
            current_limit_index=words[1],
            voltage_limit_index=words[2],
            access_level=words[3],
            wiring_scheme=words[4],
        )

    # -- analog inputs -------------------------------------------------------
    def get_analog_inputs(self) -> AnalogInputs:
        """Uses Mode 3 (full calc) because the fields needed here are split
        across Mode 4 (RMS) and Mode 5 (angles + frequency) - see
        mindflow.md "Mode coverage gap". Mode 3 is slower (up to ~3s per
        the docs) but guarantees every field we need is populated."""
        payload = self._call(c.Mode.CALC_ALL)
        if len(payload) != c.RESPONSE_PAYLOAD_BYTES:
            raise EMonProtocolError(
                f"unexpected analog-inputs payload size: {len(payload)}, "
                f"expected {c.RESPONSE_PAYLOAD_BYTES}"
            )

        def rms(channel: str) -> float:
            off = c.channel_block_byte_offset(channel) + c.FIELD5_RMS_WORD_OFFSET * 2
            # NOTE: default 12/20-bit split; clamp/external-amplifier setups
            # use a 16/16 split instead (÷65536) - see mindflow.md open
            # question #5.
            return codecs.decode_ufixed(payload[off : off + 4], frac_bits=20)

        def angle(channel: str) -> float:
            off = c.channel_block_byte_offset(channel) + c.FIELD11_ANGLE_UI_WORD_OFFSET * 2
            return codecs.decode_signed_fixed(payload[off : off + 4], frac_bits=16, total_bits=32)

        freq_off = c.FOOTER_BYTE_OFFSET + c.FIELD33_FREQUENCY_FOOTER_WORD_OFFSET * 2
        frequency = codecs.decode_ufixed(payload[freq_off : freq_off + 4], frac_bits=16)

        return AnalogInputs(
            Ia=rms("Ia"),
            Ib=rms("Ib"),
            Ic=rms("Ic"),
            Ua=rms("Ua"),
            Ub=rms("Ub"),
            Uc=rms("Uc"),
            frequency=frequency,
            phi_ia_ua=angle("Ia"),
            phi_ib_ub=angle("Ib"),
            phi_ic_uc=angle("Ic"),
        )
