"""Smoke-test the serial connection (md/plan.md step 3.1).

With no flags: just open and close the port, confirming pyserial can talk
to the OS-level device at all.

With --loopback: short the adapter's TX pin directly to its own RX pin
with a jumper wire, WITHOUT the device connected at all, then run this.
It writes a known byte pattern and checks whether it reads back exactly
what it sent. This isolates "the adapter/driver/port is fine" from any
question about the device-side wiring - a bad crimp, wrong pinout, or a
TX/RX cross on the device side will not affect this test at all, since
the device isn't part of the loop. Do this FIRST if you're not sure the
wiring to the device is right; if loopback fails, fix that before doing
anything else, since --probe below can't tell "wrong baud" apart from
"wrong wiring" - it just sees silence either way.

With --probe: (device attached, no jumper) send a Mode 3 request (using
the default CRC16 guess, which may well be wrong - that's fine here) and
print whatever raw bytes come back within the timeout, so you can eyeball
whether the device answers at all at this baud rate before worrying about
CRC correctness. Try a few common baud rates (9600, 19200, 38400, 57600,
115200) with --probe until you see plausible-looking bytes back (should
start with 0x00 or 0xff, then 0x02). If nothing ever comes back at any
baud, and --loopback already passed, suspect TX/RX swapped on the device
side (or wrong logic levels/GND not connected) rather than baud rate.

Usage:
    python scripts/01_test_connect.py /dev/ttyUSB0 --loopback
    python scripts/01_test_connect.py /dev/ttyUSB0 --baud 9600 --probe
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
    mode_group = ap.add_mutually_exclusive_group()
    mode_group.add_argument(
        "--loopback",
        action="store_true",
        help="local loopback test (TX jumpered to RX, no device attached)",
    )
    mode_group.add_argument(
        "--probe",
        action="store_true",
        help="send a Mode 3 request to the attached device and dump the raw reply",
    )
    args = ap.parse_args()

    transport = SerialTransport(args.port, args.baud, args.timeout)
    print(f"opened {args.port} @ {args.baud} baud: is_open={transport.is_open}")

    if args.loopback:
        run_loopback(transport)
    elif args.probe:
        run_probe(transport)

    transport.close()
    print("closed.")


def run_loopback(transport: SerialTransport) -> None:
    pattern = bytes(range(16)) + b"EMon-loopback-test"
    print(f"loopback: writing {len(pattern)} bytes: {pattern.hex(' ')}")
    transport.write_raw(pattern)
    echoed = transport.read_raw(len(pattern))
    if echoed == pattern:
        print("LOOPBACK OK - adapter/driver/port are fine.")
        print("Any silence you see later with the device attached is a device-side")
        print("issue (baud rate, TX/RX polarity, logic levels, GND) - not this PC side.")
    else:
        print(f"LOOPBACK FAILED - got {len(echoed)} bytes back: {echoed.hex(' ') if echoed else '(nothing)'}")
        print("Double-check TX is actually jumpered straight to RX on the adapter.")
        print("If it is and this still fails, suspect the port name, permissions, or")
        print("a broken adapter - fix this before testing against the device at all.")


def run_probe(transport: SerialTransport) -> None:
    request = build_request(Mode.CALC_ALL)
    print(f"sending: {request.hex(' ')}")
    transport.write_raw(request)
    raw = transport.read_raw(4096)
    if raw:
        print(f"received {len(raw)} bytes: {raw.hex(' ')}")
    else:
        print("received nothing - wrong baud rate, wrong TX/RX wiring, or device needs")
        print("different framing/timing. Run --loopback first if you haven't, to rule")
        print("out anything on this end of the cable.")


if __name__ == "__main__":
    main()
