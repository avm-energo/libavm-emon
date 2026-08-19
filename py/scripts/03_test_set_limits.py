"""Manual test for EMonDevice.set_current_limits() (md/plan.md step 3.5).

Usage:
    python scripts/03_test_set_limits.py /dev/ttyUSB0 \
        --limit-type 0 --current-index 0 --voltage-index 0 [--baud 9600] [--crc MODBUS]
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from emon import EMonDevice, EMonDeviceError, EMonProtocolError, crc
from emon.transport import DEFAULT_BAUDRATE


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("port")
    ap.add_argument("--baud", type=int, default=DEFAULT_BAUDRATE)
    ap.add_argument("--crc", default=crc.DEFAULT_VARIANT.name, choices=sorted(crc.VARIANTS))
    ap.add_argument("--limit-type", type=int, required=True, help="0=transformer, 1=clamp, 2=UPTT")
    ap.add_argument("--current-index", type=int, required=True)
    ap.add_argument("--voltage-index", type=int, required=True)
    args = ap.parse_args()

    device = EMonDevice(args.port, args.baud, crc.VARIANTS[args.crc])
    with device:
        try:
            applied = device.set_current_limits(args.limit_type, args.current_index, args.voltage_index)
        except (EMonProtocolError, EMonDeviceError) as exc:
            print(f"FAILED: {exc}")
            raise SystemExit(1)

    print("OK - device accepted the new limits (NORMA). Applied:")
    print(f"  current_limit_type  = {applied.current_limit_type}")
    print(f"  current_limit_index = {applied.current_limit_index}")
    print(f"  voltage_limit_index = {applied.voltage_limit_index}")
    print(f"  access_level        = {applied.access_level}")
    print(f"  wiring_scheme       = {applied.wiring_scheme:#05b}")


if __name__ == "__main__":
    main()
