"""Smoke-test the serial connection (md/plan.md step 3.1).

With no flags: just open and close the port, confirming pyserial can talk
to the OS-level device at all.

With --probe: additionally send a Mode 3 request (using the default CRC16
guess, which may well be wrong - that's fine here) and print whatever raw
bytes come back within the timeout, so you can eyeball whether the device
answers at all at this baud rate before worrying about CRC correctness.
Try a few common baud rates (9600, 19200, 38400, 57600, 115200) with
--probe until you see plausible-looking bytes back (should start with
0x00 or 0xff, then 0x02).

Usage:
    python scripts/01_test_connect.py /dev/ttyUSB0 [--baud 9600] [--probe]
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from emon.constants import Mode
from emon.frame import build_request
from emon.transport import DEFAULT_BAUDRATE, SerialTransport


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUDRATE)
    ap.add_argument("--timeout", type=float, default=2.0)
    ap.add_argument("--probe", action="store_true", help="also send a Mode 3 request and dump the raw reply")
    args = ap.parse_args()

    transport = SerialTransport(args.port, args.baud, args.timeout)
    print(f"opened {args.port} @ {args.baud} baud: is_open={transport.is_open}")

    if args.probe:
        request = build_request(Mode.CALC_ALL)
        print(f"sending: {request.hex(' ')}")
        transport.write_raw(request)
        raw = transport.read_raw(4096)
        if raw:
            print(f"received {len(raw)} bytes: {raw.hex(' ')}")
        else:
            print("received nothing - wrong baud rate, or device needs different framing/timing")

    transport.close()
    print("closed.")


if __name__ == "__main__":
    main()
