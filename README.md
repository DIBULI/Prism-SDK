# Prism Host SDK 0.11.0

[![Build SDK Examples](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml)

[中文说明](README.zh-CN.md)

This repository is the binary distribution of the Prism Host SDK. It contains
public C++ headers, prebuilt dynamic libraries for the three supported host
platforms, end-user documentation, and a CMake example. It does not contain
the SDK implementation or device firmware source code.

## Package contents

```text
Prism-SDK/
├── include/prism/                 Public C++17 headers
├── runtime/
│   ├── linux-x64/                 Ubuntu 24.04 x86-64 shared library
│   ├── macos-arm64/               macOS 13+ Apple Silicon dylibs
│   └── windows-x64/               Windows 10/11 x64 DLL
├── docs/                          Installation and usage guides
├── examples/                      Device information and time-sync example
├── CMakeLists.txt
├── ORIGIN.md                      Release provenance
└── SHA256SUMS                     Package integrity hashes
```

## Compatibility

- Host SDK: `0.11.0`
- Runtime API: `4`
- USB protocol: `10`
- Device Agent: exactly `0.11.0`
- Language: C++17 or later
- CMake: 3.20 or later

The SDK intentionally performs a strict version handshake. Do not mix headers,
dynamic libraries, or Agent firmware from different releases.

GitHub Actions automatically compiles and runtime-smoke-tests the included
example on all three supported host platforms.

## Build the example

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The build copies the required runtime library next to the example executable.
On macOS it also copies the bundled libusb dylib.

Run the example without changing device time:

```bash
./build/examples/prism-device-info-time-sync
```

Run the same example and synchronize device time to the host:

```bash
./build/examples/prism-device-info-time-sync --sync-time
```

On multi-configuration generators, such as Visual Studio, the executable is
under the selected configuration directory (for example `Release`).

Time synchronization changes RK `CLOCK_REALTIME`, the Ethernet PTP hardware
clock, and the RK RTC. All Camera, IMU, and LiDAR streams must be stopped, and
the host clock must be correct before using `--sync-time`. It does not replace
the sensor-board GPS/NMEA and PPS synchronization source.

## Documentation

- [Installation guide](docs/installation.md)
- [SDK usage guide](docs/usage.md)
- [安装指南](docs/installation.zh-CN.md)
- [SDK 使用指南](docs/usage.zh-CN.md)
- [Example guide](examples/README.md)
