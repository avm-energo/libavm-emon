"""Manual test for EMonDevice.get_analog_inputs() (md/plan.md step 3.6).

Uses Mode 3 (full calc), documented worst-case response time ~3s - see
mindflow.md "Mode coverage gap" for why Mode 4/5 alone aren't enough.

Usage:
    python scripts/04_test_get_analog_inputs.py /dev/ttyUSB0 [--baud 9600] [--crc MODBUS]
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
    ap.add_argument("--timeout", type=float, default=4.0, help="Mode 3 can take up to ~3s")
    ap.add_argument("--crc", default=crc.DEFAULT_VARIANT.name, choices=sorted(crc.VARIANTS))
    args = ap.parse_args()

    device = EMonDevice(args.port, args.baud, crc.VARIANTS[args.crc])
    device.connect(timeout=args.timeout)  # Mode 3 needs a longer timeout than the default
    try:
        try:
            values = device.get_analog_inputs()
        except (EMonProtocolError, EMonDeviceError) as exc:
            print(f"FAILED: {exc}")
            raise SystemExit(1)
    finally:
        device.disconnect()

    print(f"Ia = {values.Ia:.4f} A   Ua = {values.Ua:.4f} V   phi(Ia/Ua) = {values.phi_ia_ua:.4f} rad")
    print(f"Ib = {values.Ib:.4f} A   Ub = {values.Ub:.4f} V   phi(Ib/Ub) = {values.phi_ib_ub:.4f} rad")
    print(f"Ic = {values.Ic:.4f} A   Uc = {values.Uc:.4f} V   phi(Ic/Uc) = {values.phi_ic_uc:.4f} rad")
    print(f"frequency = {values.frequency:.4f} Hz")
    print()
    print("RMS values (Ia/Ib/Ic/Ua/Ub/Uc) are the device's raw pre-calibration")
    print("reading, not its own front-panel's calibrated value - see get_analog_inputs()'s")
    print("docstring. phi's field offset is still unconfirmed against real hardware -")
    print("see mindflow.md \"Step 4.3\", open question 5b.")


if __name__ == "__main__":
    main()
