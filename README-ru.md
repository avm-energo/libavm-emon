# libavm-emon

[![pipeline status](https://git.avmenergo.ru/avm-energo/libavm-emon/badges/master/pipeline.svg)](https://git.avmenergo.ru/avm-energo/libavm-emon/-/pipelines)
[![latest release](https://git.avmenergo.ru/avm-energo/libavm-emon/-/badges/release.svg)](https://git.avmenergo.ru/avm-energo/libavm-emon/-/releases)

English: [README.md](README.md)

Программа разработана ООО «АВМ-Энерго» с помощью Claude.

**libavm-emon** — библиотека протокола измерителя показателей качества электроэнергии ЭМ‑3.1КМ («EMon») для проектов
АВМ-Энерго. Общается с устройством по RS-232 или USB (по собственному USB-порту устройства, а не по предположению
об обычном виртуальном COM-порте — как это подтвердили, см. «Step 5» в `md/mindflow.md`) и реализует три функции,
которые проект сейчас поддерживает: `GetDeviceInfo`, `SetLimits`, `GetAnalogInputs`.

Параллельная реализация на Python (`py/`) использовалась для обратной разработки и проверки протокола до переноса
в эту библиотеку — полная история, включая каждую деталь протокола, в которой ошибалась документация производителя,
и как это нашли на реальном оборудовании, — в `md/mindflow.md`.

# Требования

- Инструменты:
  - компилятор C++ с полной поддержкой C++17 (MSVC 2019 16.10+, gcc 9.3.0+, clang-11+)
  - CMake 3.18+
- Зависимости:
  - Qt 6 (Core + SerialPort) — https://www.qt.io/. `find_package(QT NAMES Qt6 Qt5)` откатывается на Qt 5, если
    Qt 6 не найден, но проверялся только Qt 6.

# Сборка

```bash
$ cmake -B build -S . -DCMAKE_PREFIX_PATH=путь_к_qt
$ cmake --build build --config Release
```

Поддерживаемые опции:
- cmake
  - `CMAKE_PREFIX_PATH` — путь к установке Qt, например `C:\Qt\6.10.1\msvc2022_64`
  - `BUILD_SHARED_LIBS:BOOL=ON/OFF` — динамическая (`.dll`/`.so`) или статическая (`.lib`/`.a`) библиотека.
    Если не передать `ON`, действует умолчание CMake (`OFF`, т. е. статическая).
  - `CMAKE_BUILD_TYPE` — `Debug`/`Release`/`MinSizeRel`/`RelWithDebInfo` (по умолчанию `Release`)

`tests/` (ручная проверка на оборудовании) и `examples/` собираются только когда это корневой проект CMake — при
подключении через `add_subdirectory()`/`FetchContent` их не будет.

# Использование

```cpp
#include <libavm-emon/Device.h>

// LinkType выбирает параметры последовательного порта, реально подтверждённые для каждого соединения —
// RS-232: 115200 8N1; USB: 3 000 000 бод 8N2. Передать произвольную непроверенную комбинацию
// скорости/формата намеренно нельзя.
avm_emon::Device device("COM1", avm_emon::LinkType::Rs232);
device.connectDevice();

avm_emon::DeviceInfo info = device.getDeviceInfo();
avm_emon::AppliedLimits limits = device.setLimits(/* currentLimitType */ 0,
                                                   /* currentLimitIndex */ 3,
                                                   /* voltageLimitIndex */ 0);
avm_emon::AnalogInputs values = device.getAnalogInputs();

device.disconnectDevice();
```

Любой вызов при ошибке бросает `avm_emon::ProtocolError` или `avm_emon::DeviceError` (оба наследуют
`avm_emon::Exception`); при успешном возврате ничего проверять не нужно — CRC кадра уже проверена. Полные
рабочие примеры, включая рекомендуемый внешний повтор для `getAnalogInputs()` по USB (причина — в разделе
«Известные ограничения»), — в `examples/`.

# Поддерживаемые типы соединения

|              | RS-232  | USB           |
|--------------|---------|---------------|
| Скорость     | 115200  | 3 000 000     |
| Формат       | 8N1     | 8N2           |
| `LinkType`   | `Rs232` | `Usb`         |

Оба варианта подтверждены на реальном оборудовании — как, см. «Step 3.1» и «Step 5» в `md/mindflow.md`.

# Известные ограничения

- Ответ `getAnalogInputs()` размером ~4,5 КБ по USB иногда зависает на середине передачи — это реальная,
  подтверждённая особенность этого соединения на 3 000 000 бод (воспроизводится одинаково с совершенно
  независимой Python-библиотекой, т. е. не связана ни с этой библиотекой, ни с Qt), а не ошибка здесь.
  `Transport::request()` повторяет запрос внутри себя (по умолчанию 5 попыток), чтобы это скрыть; прикладной код,
  вызывающий `getAnalogInputs()`, всё равно должен быть готов к редкому внешнему повтору — см.
  `examples/get_analog_inputs/` и «Step 6.1» в `md/mindflow.md`.
- Поле версии ПО устройства и ряд полей гармоник/средних значений в `getAnalogInputs()` (не вынесенных в узкий API
  библиотеки) остаются нерасшифрованными — см. раздел «Open questions» в `md/mindflow.md`.

# Пакеты

Linux-пакеты собирает CI (`.gitlab-ci.yml`) через CPack: `libavm-emon` (runtime, динамическая библиотека) и
`libavm-emon-dev` (`.deb`) / `libavm-emon-devel` (`.rpm`) (статическая библиотека, заголовки, файлы CMake-пакета). Локальная сборка:

```bash
$ cmake -B build -S . -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr
$ cmake --build build
$ cd build && cpack -G DEB   # или -G RPM
```

Пакеты каждой версии (тега) перечислены на [странице релизов](https://git.avmenergo.ru/avm-energo/libavm-emon/-/releases).
В `package/` лежат исходники Debian (`debian`/`debian-dev`) и RPM (`rpm/rpmbuild`) для сборок в TeamCity —
по принятому в других C++-библиотеках АВМ-Энерго соглашению.
