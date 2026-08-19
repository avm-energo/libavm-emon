"""Serial transport for the EMon protocol.

NOTE: the vendor docs never state the UART parameters (baud rate, parity,
stop bits) - only an opaque "current speed index" field inside the
device's system memory area. `DEFAULT_BAUDRATE` below is CONFIRMED against
real hardware (md/mindflow.md "Step 3.1" resolution): 115200 8N1.
"""
from __future__ import annotations

import serial

DEFAULT_BAUDRATE = 115200
DEFAULT_TIMEOUT_S = 2.0
# Also unverified - the docs say nothing about framing beyond baud, so
# these are just the overwhelmingly common defaults, kept overridable.
DEFAULT_BYTESIZE = serial.EIGHTBITS
DEFAULT_PARITY = serial.PARITY_NONE
DEFAULT_STOPBITS = serial.STOPBITS_ONE


class SerialTransport:
    def __init__(
        self,
        port: str,
        baudrate: int = DEFAULT_BAUDRATE,
        timeout: float = DEFAULT_TIMEOUT_S,
        bytesize: int = DEFAULT_BYTESIZE,
        parity: str = DEFAULT_PARITY,
        stopbits: float = DEFAULT_STOPBITS,
    ):
        self._ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=bytesize,
            parity=parity,
            stopbits=stopbits,
            timeout=timeout,
        )

    def close(self) -> None:
        if self._ser.is_open:
            self._ser.close()

    @property
    def is_open(self) -> bool:
        return self._ser.is_open

    @property
    def timeout(self) -> float:
        return self._ser.timeout

    @timeout.setter
    def timeout(self, value: float) -> None:
        self._ser.timeout = value

    def write_raw(self, data: bytes) -> None:
        """Write bytes without expecting/parsing any particular response -
        for probing an unknown baud rate or CRC (see scripts/01, 00)."""
        self._ser.reset_input_buffer()
        self._ser.write(data)

    def read_raw(self, max_bytes: int) -> bytes:
        """Best-effort read of whatever arrives within the timeout; may
        return fewer than `max_bytes`, including zero."""
        return self._ser.read(max_bytes)

    def request(self, frame: bytes, retries: int = 1) -> bytes:
        """Send `frame` and read back exactly one response frame, using the
        length field to know how many bytes to read.

        CONFIRMED against real hardware (md/mindflow.md "Step 4.6"): under
        back-to-back polling, response latency is otherwise a remarkably
        steady ~2.5s, but roughly 1-in-15 requests hit a genuine device-side
        stall - observed taking 15s+ and still coming up a few bytes short
        even with a generous timeout, not just "slightly exceeded a tight
        one". A plain retry (fresh write, fresh read) has recovered every
        single time this was tested manually, so that's what `retries`
        does here - it is not a workaround for a bug in this code, it's
        coping with a real intermittent device hiccup. Pass `retries=0` to
        disable and get the old fail-fast behaviour."""
        attempts = retries + 1
        last_exc: TimeoutError | None = None
        for attempt in range(attempts):
            self._ser.reset_input_buffer()
            self._ser.write(frame)
            try:
                header = self._read_exact(4)
                length = int.from_bytes(header[2:4], "little")
                rest = self._read_exact(length + 2)  # payload + CRC
                return header + rest
            except TimeoutError as exc:
                last_exc = exc
                if attempt + 1 < attempts:
                    continue
                raise
        raise last_exc  # unreachable, satisfies type checkers

    def _read_exact(self, n: int) -> bytes:
        data = self._ser.read(n)
        if len(data) != n:
            raise TimeoutError(
                f"expected {n} bytes within {self._ser.timeout}s, got {len(data)}: {data!r}"
            )
        return data
