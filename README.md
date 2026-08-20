# Prism Host SDK 1.0.0

[![Build SDK Examples](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml)

[中文说明](README.zh-CN.md)

This repository is the binary distribution of the Prism Host SDK. It contains
public C++ headers, prebuilt dynamic libraries for the three supported host
platforms, end-user documentation, and CMake examples. It does not contain
the SDK implementation or device firmware source code.

## Package contents

```text
Prism-SDK/
├── include/prism/                 Public C++17 headers
├── runtime/
│   ├── linux-x64/                 Default Ubuntu 22.04+ x86-64 library
│   ├── linux-arm64/               Default Ubuntu 22.04+ ARM64 library
│   ├── ros/
│   │   ├── ubuntu-20.04-x86_64/   ROS 1 Noetic SDK prefix
│   │   ├── ubuntu-22.04-x86_64/   ROS 2 Humble SDK prefix
│   │   ├── ubuntu-24.04-x86_64/   ROS 2 Jazzy/Kilted SDK prefix
│   │   └── ubuntu-26.04-x86_64/   ROS 2 Lyrical/Rolling SDK prefix
│   ├── macos-arm64/               macOS 13+ Apple Silicon dylibs
│   └── windows-x64/               Windows 10/11 x64 DLL
├── docs/                          Installation and usage guides
├── examples/                      Compile-tested SDK examples
├── CMakeLists.txt
├── ORIGIN.md                      Release provenance
└── SHA256SUMS                     Package integrity hashes
```

## Compatibility

- Host SDK: `1.0.0`
- Runtime API: `4`
- USB protocol: `1`
- Device Agent: exactly `1.0.0`
- Language: C++17 or later
- CMake: 3.20 or later

The SDK intentionally performs a strict version handshake. Do not mix headers,
dynamic libraries, or Agent firmware from different releases.

GitHub Actions compiles every example source across the three-platform matrix,
runs all no-device support tests, and runtime-smoke-tests the published dynamic
libraries. CMake rejects an unregistered `examples/*.cpp` source, preventing a
future example from silently escaping CI.

Both Linux architectures are built against the same Ubuntu 22.04 ABI baseline.
The repository also provides complete ABI-specific binary SDK prefixes under
`runtime/ros` for the ROS Adapter. The Adapter selects the correct prefix for
each ROS distribution; desktop SDK consumers use
`runtime/linux-x64/libprism_usb_sdk.so` by default.

## Build the examples

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The build copies the required runtime library next to the example executable.
On macOS it also copies the bundled libusb dylib.

Run the full automated package, build-target, and CTest verification used by
GitHub Actions:

```bash
python3 scripts/test_all_examples.py --build-dir build-all-examples
```

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

- [Complete SDK development guide](docs/development-guide.md)
- [Per-interface SDK examples](docs/interface-examples.md)
- [完整 SDK 开发手册](docs/development-guide.zh-CN.md)
- [逐接口 SDK 示例](docs/interface-examples.zh-CN.md)
- [Installation guide](docs/installation.md)
- [SDK usage guide](docs/usage.md)
- [安装指南](docs/installation.zh-CN.md)
- [SDK 使用指南](docs/usage.zh-CN.md)
- [Example guide](examples/README.md)
