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


class SerialTransport:
    def __init__(self, port: str, baudrate: int = DEFAULT_BAUDRATE, timeout: float = DEFAULT_TIMEOUT_S):
        self._ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
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

    def request(self, frame: bytes) -> bytes:
        """Send `frame` and read back exactly one response frame, using the
        length field to know how many bytes to read."""
        self._ser.reset_input_buffer()
        self._ser.write(frame)
        header = self._read_exact(4)
        length = int.from_bytes(header[2:4], "little")
        rest = self._read_exact(length + 2)  # payload + CRC
        return header + rest

    def _read_exact(self, n: int) -> bytes:
        data = self._ser.read(n)
        if len(data) != n:
            raise TimeoutError(
                f"expected {n} bytes within {self._ser.timeout}s, got {len(data)}: {data!r}"
            )
        return data
