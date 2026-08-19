"""One-off capture helper: log all raw bytes seen on a mirrored/split
virtual serial port (e.g. via HHD Virtual Serial Port Tools "Split Port"
pointed at the real port the vendor software is using), with timestamps,
so a real request/response exchange can be captured for protocol
reverse-engineering (md/plan.md step 3.1/3.2).

Not part of the emon package - throwaway diagnostic script.

Usage:
    python scripts/99_sniff_mirror.py COM3 --seconds 90 --baud 115200
"""
import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import serial


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=90.0)
    args = ap.parse_args()

    ser = serial.Serial(
        port=args.port,
        baudrate=args.baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.1,
    )
    print(f"listening on {args.port} @ {args.baud} baud for {args.seconds}s ...")
    start = time.time()
    events = []
    while time.time() - start < args.seconds:
        chunk = ser.read(4096)
        if chunk:
            t = time.time() - start
            events.append((t, chunk))
            print(f"[{t:7.3f}s] {len(chunk):4d} bytes: {chunk.hex(' ')}")
    ser.close()

    if not events:
        print("no bytes captured at all - port/baud mismatch, or no traffic occurred during the window")
    else:
        print(f"\ncaptured {len(events)} chunk(s), {sum(len(c) for _, c in events)} bytes total")


if __name__ == "__main__":
    main()
