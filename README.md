# libavm-emon

[![pipeline status](https://git.avmenergo.ru/avm-energo/libavm-emon/badges/master/pipeline.svg)](https://git.avmenergo.ru/avm-energo/libavm-emon/-/pipelines)
[![latest release](https://git.avmenergo.ru/avm-energo/libavm-emon/-/badges/release.svg)](https://git.avmenergo.ru/avm-energo/libavm-emon/-/releases)

Русская версия: [README-ru.md](README-ru.md)

This software was made by AVM-Energo LLC with the help of Claude.

**libavm-emon** is a protocol library for the ЭМ‑3.1КМ ("Energomonitor") power-quality meter, for
AVM-Energo projects. It talks to the device over either RS-232 or USB (the
device's own USB port, not a generic virtual-COM assumption and implements the
three functions this project currently targets: `GetDeviceInfo`,
`SetLimits`, `GetAnalogInputs`.

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


# Packaging

Linux packages are built by CI (`.gitlab-ci.yml`) with CPack: `libavm-emon` (runtime, the shared library) and
`libavm-emon-dev` (`.deb`) / `libavm-emon-devel` (`.rpm`) (static library, headers, CMake package files). To build them
locally:

```bash
$ cmake -B build -S . -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr
$ cmake --build build
$ cd build && cpack -G DEB   # or -G RPM
```

Packages of every tagged version are listed on the [Releases page](https://git.avmenergo.ru/avm-energo/libavm-emon/-/releases).
