"""Brute-force the EMon CRC16 variant against a real device (md/plan.md
step 3.2).

Sends a cheap Mode 3 request (empty payload, read-only, safe to repeat)
once per candidate CRC16 variant; whichever variant gets back a
well-formed response (right sync byte, CRC checks out) is almost
certainly the one the device uses on the wire.

Run scripts/01_test_connect.py first to make sure the baud rate is right
- if the baud is wrong, every variant here will just time out and you
won't be able to tell "wrong baud" from "wrong CRC".

Usage:
    python scripts/00_find_crc.py /dev/ttyUSB0 [--baud 9600] [--timeout 2]
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from emon import crc
from emon.constants import Mode
from emon.frame import EMonProtocolError, build_request, parse_response
from emon.transport import DEFAULT_BAUDRATE, SerialTransport


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="serial port, e.g. /dev/ttyUSB0 or COM3")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUDRATE)
    ap.add_argument("--timeout", type=float, default=2.0)
    args = ap.parse_args()

    transport = SerialTransport(args.port, args.baud, args.timeout)
    try:
        for name, variant in crc.VARIANTS.items():
            request = build_request(Mode.CALC_ALL, b"", variant)
            print(f"trying {name}: request={request.hex(' ')}")
            try:
                raw = transport.request(request)
            except TimeoutError as exc:
                print(f"  no/short response ({exc}) - skipping")
                continue
            try:
                response = parse_response(raw, variant)
            except EMonProtocolError as exc:
                print(f"  response received but doesn't check out: {exc}")
                continue
            print(f"  MATCH: variant={name!r} ok={response.ok} payload_len={len(response.payload)}")
            print(f"  raw response: {raw.hex(' ')}")
            return
        print("No candidate variant produced a well-formed response.")
        print("Check the baud rate first (scripts/01_test_connect.py --probe), or the")
        print("device may use a CRC16 variant not in emon.crc.VARIANTS.")
    finally:
        transport.close()


if __name__ == "__main__":
    main()
