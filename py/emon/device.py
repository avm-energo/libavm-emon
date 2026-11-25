"""High level EMon device API.

Implements the four functions targeted for the first milestone:
connect/disconnect, get_device_info, set_current_limits, get_analog_inputs.
Everything here is unverified against real hardware yet - see
md/plan.md step 3 and md/mindflow.md for the open questions.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional, Tuple

from . import codecs
from . import constants as c
from . import crc
from .frame import EMonDeviceError, EMonProtocolError, build_request, parse_response
from .transport import DEFAULT_BAUDRATE, DEFAULT_TIMEOUT_S, SerialTransport


@dataclass
class DeviceInfo:
    device_type: str
    serial_number: int
    # DAA-digit decode of each raw word (same trick as decode_daa_date),
    # e.g. raw (0x0702, 0x0153) -> "702.153" - confirmed against the
    # device's own display, which shows this combined with the serial
    # number as "702.153.519" (see mindflow.md "Step 8"). The display
    # separately shows a second, shorter version ("2.01" on the unit
    # tested) that this field does NOT decode from anywhere in this
    # 512-byte area - likely a separate bootloader/hardware version not
    # exposed by this protocol at all.
    sw_version: str
    sw_version_raw_words: Tuple[int, int]
    # SW version date (Y-M-D), same DAA-digit decode, immediately
    # following the version words - see mindflow.md "Step 8".
    sw_version_date: Tuple[int, int, int]
    device_type_id: int


@dataclass
class AppliedLimits:
    current_limit_type: int
    current_limit_index: int
    voltage_limit_index: int
    access_level: int
    wiring_scheme: int  # 0b001/0b010/0b100 = 3-phase-4-wire/3-phase-3-wire/1-phase-2-wire


@dataclass
class SupportedLimits:
    """The device's supported current/voltage ranges, decoded from the
    system area's variable-length limits table - see mindflow.md "Step 9".
    Each list is index-aligned with the device's own indexing for that
    category (i.e. `voltage[2]` is what `voltage_limit_index=2` selects).

    `voltage` and `current_transformer` are CONFIRMED against real
    hardware (19/19 exact matches against a real unit's own displayed
    limit lists). `current_clamp`/`current_uptt` use the same decode
    formula as `current_transformer` (same physical quantity, same
    protocol) but that specific assumption is NOT independently
    confirmed - no clamp/UPTT hardware was available to check against."""

    voltage: List[float]
    current_transformer: List[float]
    current_clamp: List[float]
    current_uptt: List[float]

    def current_list_for(self, current_limit_type: int) -> List[float]:
        """`current_limit_type` in Mode 2's own numbering (0=transformer,
        1=clamp, 2=UPTT), matching `set_current_limits`'s parameter."""
        return {
            0: self.current_transformer,
            1: self.current_clamp,
            2: self.current_uptt,
        }[current_limit_type]


@dataclass
class AnalogInputs:
    Ia: float
    Ib: float
    Ic: float
    Ua: float
    Ub: float
    Uc: float
    # Hz, precise to about 1e-3: footer words 40-41, 16.16 with the high word
    # first, hardware-verified at 50 / 55 / 52.347 Hz (mindflow.md "Step 13").
    # (Footer word 39, used in earlier versions, is coarse and ~1.5% low.)
    frequency: float
    # DEGREES, not radians - confirmed against a real inductive-load test,
    # see mindflow.md "Step 4.4". `decode_signed_fixed`'s frac_bits=16
    # format guess was right; the field's physical unit was the wrong
    # assumption, inherited from the "угол"/"angle" field name without a
    # hardware check.
    phi_ia_ua: float
    phi_ib_ub: float
    phi_ic_uc: float
    # The limit configuration active when this reading was taken (fields
    # 45/46, footer word offsets 64/65 - CONFIRMED unshifted from the doc
    # against Mode 2's own echo, see mindflow.md "Step 4.2"). Needed to
    # know which range the reading was taken in - see mindflow.md "Step 11".
    applied_current_limit_type: int  # Mode 2 numbering: 0=transformer,1=clamp,2=UPTT
    applied_current_limit_index: int
    applied_voltage_limit_index: int
    # Angles between phase voltages, degrees: UA1-UB1, UB1-UC1, UC1-UA1 (mindflow.md "Step 12").
    phi_ua_ub: float = 0.0
    phi_ub_uc: float = 0.0
    phi_uc_ua: float = 0.0


@dataclass
class CalibratedAnalogInputs:
    """Same values as `AnalogInputs` (kept for compatibility) - see `calibrate()`."""

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
    phi_ua_ub: float = 0.0
    phi_ub_uc: float = 0.0
    phi_uc_ua: float = 0.0


def calibrated_current(raw_amps: float, limits: SupportedLimits, current_limit_type: int, current_limit_index: int) -> float:
    """Returns `raw_amps` unchanged (arguments kept for compatibility).
    An earlier version scaled by the configured range as if it were a CT
    ratio; that was wrong - the ranges are real measurement ranges (device
    manual 4.2.3) and the reported RMS is already in amperes. See
    mindflow.md "Step 11"."""
    return raw_amps


def calibrated_voltage(raw_volts: float) -> float:
    """CONFIRMED against real hardware (mindflow.md "Step 4.3"/"Step 9"):
    voltage needs no scaling at all - the raw reading already matches the
    front panel exactly, regardless of the configured voltage range
    (which appears to just select ADC gain/precision, not a ratio)."""
    return raw_volts


def calibrate(raw: AnalogInputs, limits: SupportedLimits) -> CalibratedAnalogInputs:
    """Copies every channel unchanged (kept for compatibility, see `calibrated_current`)."""
    return CalibratedAnalogInputs(
        Ia=raw.Ia,
        Ib=raw.Ib,
        Ic=raw.Ic,
        Ua=calibrated_voltage(raw.Ua),
        Ub=calibrated_voltage(raw.Ub),
        Uc=calibrated_voltage(raw.Uc),
        frequency=raw.frequency,
        phi_ia_ua=raw.phi_ia_ua,
        phi_ib_ub=raw.phi_ib_ub,
        phi_ic_uc=raw.phi_ic_uc,
        phi_ua_ub=raw.phi_ua_ub,
        phi_ub_uc=raw.phi_ub_uc,
        phi_uc_ua=raw.phi_uc_ua,
    )


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
        sw_version = "{}.{}".format(
            int(codecs.decode_daa_digit_string(sw_version_raw_words[0], 4)),
            int(codecs.decode_daa_digit_string(sw_version_raw_words[1], 4)),
        )
        sw_version_date = codecs.decode_daa_date(payload[22:24], payload[24:26])
        return DeviceInfo(
            device_type, serial_number, sw_version, sw_version_raw_words, sw_version_date, device_type_id
        )

    def get_supported_limits(self) -> SupportedLimits:
        """Read and decode the variable-length supported-limits table
        (system area offset 44 onward, terminated by a type=0 entry) -
        see mindflow.md "Step 9"."""
        payload = self._read_fram(start_word=0, count_words=256)
        table = payload[c.LIMITS_TABLE_BYTE_OFFSET :]
        entries = codecs.decode_limits_table(table, c.LIMIT_TABLE_DIVISOR)
        by_type: dict = {}
        for entry_type, value in entries:
            by_type.setdefault(entry_type, []).append(value)
        return SupportedLimits(
            voltage=by_type.get(c.LimitTableType.VOLTAGE, []),
            current_transformer=by_type.get(c.LimitTableType.CURRENT_TRANSFORMER, []),
            current_clamp=by_type.get(c.LimitTableType.CURRENT_CLAMP, []),
            current_uptt=by_type.get(c.LimitTableType.CURRENT_UPTT, []),
        )

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
        the docs) but guarantees every field we need is populated.

        NOTE: Ia/Ib/Ic/Ua/Ub/Uc are RMS values in amperes/volts exactly as
        the device reports them and match its front panel; the configured
        ranges are measurement ranges, not a ratio (mindflow.md "Step 11")."""
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

        def uu_angle(channel: str) -> float:
            off = c.channel_block_byte_offset(channel) + c.FIELD12_ANGLE_UU_WORD_OFFSET * 2
            return codecs.decode_signed_fixed(payload[off : off + 4], frac_bits=16, total_bits=32)

        freq_off = c.FOOTER_BYTE_OFFSET + c.FIELD33_FREQUENCY_FOOTER_WORD_OFFSET * 2
        # 16.16, HIGH word first (see constants.py), so swap the two words.
        fw = payload[freq_off : freq_off + 4]
        frequency = codecs.decode_ufixed(fw[2:4] + fw[0:2], frac_bits=16)

        limit45_off = c.FOOTER_BYTE_OFFSET + c.FIELD45_CURRENT_LIMIT_FOOTER_WORD_OFFSET * 2
        limit45 = int.from_bytes(payload[limit45_off : limit45_off + 2], "little")
        applied_current_limit_type = (limit45 >> 8) & 0xFF
        applied_current_limit_index = limit45 & 0xFF

        limit46_off = c.FOOTER_BYTE_OFFSET + c.FIELD46_VOLTAGE_LIMIT_FOOTER_WORD_OFFSET * 2
        applied_voltage_limit_index = int.from_bytes(payload[limit46_off : limit46_off + 2], "little")

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
            applied_current_limit_type=applied_current_limit_type,
            applied_current_limit_index=applied_current_limit_index,
            applied_voltage_limit_index=applied_voltage_limit_index,
            phi_ua_ub=uu_angle("Ua"),
            phi_ub_uc=uu_angle("Ub"),
            phi_uc_ua=uu_angle("Uc"),
        )
