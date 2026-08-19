# libavm-emon

Protocol library for the ЭМ‑3.1КМ ("EMon") power-quality meter, for
AVM-Energo projects. Talks to the device over either RS-232 or USB (the
device's own USB port, not a generic virtual-COM assumption - see
`md/mindflow.md` "Step 5" for how that was confirmed) and implements the
three functions this project currently targets: `GetDeviceInfo`,
`SetLimits`, `GetAnalogInputs`.

A parallel Python implementation (`py/`) was used to reverse-engineer and
validate the protocol before this library was ported from it - see
`md/mindflow.md` for the full story, including every protocol detail the
vendor documentation got wrong and how each was found against real
hardware.

# Requires

- Tools:
  - C++ compiler with full C++17 support (MSVC 2019 16.10+, gcc 9.3.0+, clang-11+)
  - CMake 3.18+
- Dependencies:
  - Qt 6 (Core + SerialPort) - https://www.qt.io/. `find_package(QT NAMES Qt6 Qt5)`
    falls back to Qt 5 if 6 isn't found, but only Qt 6 has been tested.

# Build

```bash
$ cmake -B build -S . -DCMAKE_PREFIX_PATH=path_to_qt
$ cmake --build build --config Release
```

supported options:
- cmake
  - `CMAKE_PREFIX_PATH` - path to your Qt install, e.g. `C:\Qt\6.10.1\msvc2022_64`
  - `BUILD_SHARED_LIBS:BOOL=ON/OFF` - shared (`.dll`/`.so`) or static
    (`.lib`/`.a`) library. CMake's own default (`OFF`, i.e. static)
    applies unless you pass `ON`.
  - `CMAKE_BUILD_TYPE` - `Debug`/`Release`/`MinSizeRel`/`RelWithDebInfo`
    (defaults to `Release` if unset)

`tests/` (a manual hardware-validation tool) and `examples/` are only
built when this is the top-level CMake project - a consumer pulling this
library in via `add_subdirectory()`/`FetchContent` won't get them.

# Usage

```cpp
#include <libavm-emon/Device.h>

// LinkType picks the serial parameters actually confirmed to work for
// each connection - RS-232: 115200 8N1; USB: 3,000,000 baud 8N2. There's
// deliberately no way to pass a raw, untested baud/framing combination.
avm_emon::Device device("COM1", avm_emon::LinkType::Rs232);
device.connectDevice();

avm_emon::DeviceInfo info = device.getDeviceInfo();
avm_emon::AppliedLimits limits = device.setLimits(/* currentLimitType */ 0,
                                                   /* currentLimitIndex */ 3,
                                                   /* voltageLimitIndex */ 0);
avm_emon::AnalogInputs values = device.getAnalogInputs();

device.disconnectDevice();
```

Every call throws `avm_emon::ProtocolError` or `avm_emon::DeviceError`
(both derive from `avm_emon::Exception`) on failure - there's nothing to
manually check on a successful return, the frame's CRC has already been
validated before the call returns. See `examples/` for complete,
runnable programs, including the outer-retry pattern recommended for
`getAnalogInputs()` over USB (see "Known limitations" below for why).

# Supported link types

|            | RS-232  | USB           |
|------------|---------|---------------|
| Baud rate  | 115200  | 3,000,000     |
| Framing    | 8N1     | 8N2           |
| `LinkType` | `Rs232` | `Usb`         |

Both confirmed against real hardware - see `md/mindflow.md` "Step 3.1"
and "Step 5" for how.

# Known limitations

- `getAnalogInputs()`'s ~4.5KB response occasionally stalls mid-transfer
  over USB - a real, confirmed characteristic of that link at 3,000,000
  baud (reproduced identically with a completely independent Python
  serial library, so it isn't specific to this library or to Qt), not a
  bug here. `Transport::request()` retries internally (5 attempts by
  default) to absorb this; application code calling `getAnalogInputs()`
  should still be prepared for an occasional outer-level retry - see
  `examples/get_analog_inputs/` and `md/mindflow.md` "Step 6.1".
- The device's SW-version field and a handful of `getAnalogInputs()`
  harmonic/mean-value fields (not exposed by this library's narrow API)
  remain unresolved - see `md/mindflow.md`'s "Open questions" section.

# Packaging

`package/` holds Debian (`debian`/`debian-dev`) and RPM (`rpm/rpmbuild`)
packaging sources, matching the convention used across AVM-Energo's other
C++ libraries (see `libavm-gen`).
