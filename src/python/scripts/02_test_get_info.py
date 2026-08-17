"""Manual test for EMonDevice.get_device_info() (md/plan.md step 3.4).

Usage:
    python scripts/02_test_get_info.py /dev/ttyUSB0 [--baud 9600] [--crc MODBUS]
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
    args = ap.parse_args()

    device = EMonDevice(args.port, args.baud, crc.VARIANTS[args.crc])
    with device:
        try:
            info = device.get_device_info()
        except (EMonProtocolError, EMonDeviceError) as exc:
            print(f"FAILED: {exc}")
            raise SystemExit(1)

    print(f"device_type       = {info.device_type!r}")
    print(f"serial_number     = {info.serial_number}")
    print(f"device_type_id    = {info.device_type_id}")
    print(f"sw_version_raw    = {info.sw_version_raw_words[0]:#06x}, {info.sw_version_raw_words[1]:#06x}")
    print("  (SW version decoding is unresolved - see mindflow.md; report the raw")
    print("   words plus what the device's own display/tool shows for the version")
    print("   so the encoding can be figured out)")


if __name__ == "__main__":
    main()
