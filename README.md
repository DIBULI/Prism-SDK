# Prism SDK Distribution 1.0.2

[![Build SDK Examples](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml)

[中文说明](README.zh-CN.md)

This repository is the binary distribution of the Prism Host SDK. It contains
public C++ headers, prebuilt dynamic libraries for the three supported host
platforms, Linux x86-64/arm64 static libraries, end-user documentation, and
CMake examples. It does not contain the SDK implementation or device firmware
source code.

## Package contents

```text
Prism-SDK/
├── include/prism/                 Public C++17 headers
├── runtime/
│   ├── linux-x64/                 Ubuntu 22.04+ x86-64 .so and .a
│   ├── linux-arm64/               Ubuntu 22.04+ arm64 .so and .a
│   ├── ros/
│   │   ├── ubuntu-20.04-x86_64/   ROS 1 Noetic SDK prefix
│   │   ├── ubuntu-22.04-x86_64/   ROS 2 Humble SDK prefix
│   │   ├── ubuntu-24.04-x86_64/   ROS 2 Jazzy/Kilted SDK prefix
│   │   ├── ubuntu-26.04-x86_64/   ROS 2 Lyrical/Rolling SDK prefix
│   │   └── linux-arm64/            All supported ROS/Ubuntu ARM64 releases
│   ├── macos-arm64/               macOS 13+ Apple Silicon dylibs
│   └── windows-x64/               Windows 10/11 x64 DLL
├── docs/                          Installation and usage guides
├── examples/                      Compile-tested SDK examples
├── CMakeLists.txt
├── ORIGIN.md                      Release provenance
└── SHA256SUMS                     Package integrity hashes
```

## Compatibility

- Distribution release: `1.0.2`
- Host SDK runtime/ABI: `1.0.0`
- Runtime API: `5`
- USB protocol: `1`
- Device Agent: exactly `1.0.0`
- Language: C++17 or later
- CMake: 3.20 or later

Release 1.0.2 packages the existing Host SDK 1.0.0 interface for all supported
platforms and adds the Linux ARM64 deliverables compiled from that same 1.0.0
SDK source baseline. The runtime intentionally performs a strict 1.0.0
SDK/Agent handshake. Do not mix headers and libraries from different
distribution releases, or use an Agent other than 1.0.0.

GitHub Actions compiles every example source across the three-platform matrix,
runs all no-device support tests, runtime-smoke-tests the published dynamic
libraries, and verifies static linking on both Linux architectures. CMake
rejects an unregistered `examples/*.cpp` source, preventing a future example
from silently escaping CI.

The normal Linux shared libraries use an Ubuntu 22.04 ABI baseline. The
repository also provides complete binary SDK prefixes under `runtime/ros` for
the ROS Adapter. x86-64 has one shared-library prefix per Ubuntu ABI. ARM64 has
one static-library prefix that is linked inside each target ROS environment,
matching the cross-Ubuntu strategy used by Prism Viewer. Desktop SDK consumers
use `runtime/linux-x64/libprism_usb_sdk.so` by default. Set
`PRISM_SDK_USE_STATIC=ON` to use the matching `libprism_usb_sdk.a` instead.

## Linux static SDK

Install the OpenSSL and libusb development packages, then enable the static SDK
option when configuring a consumer:

```bash
sudo apt-get install -y libssl-dev libusb-1.0-0-dev
cmake -S . -B build-static \
  -DCMAKE_BUILD_TYPE=Release \
  -DPRISM_SDK_USE_STATIC=ON
cmake --build build-static --config Release
```

This removes the final application's dependency on `libprism_usb_sdk.so`.
OpenSSL and libusb remain transitive dependencies and are dynamically linked by
default unless the consumer explicitly selects compatible static builds of
those projects.

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

- [Release 1.0.2 update notes](docs/update/v1.0.2.md)
- [1.0.2 更新说明](docs/update/v1.0.2.zh-CN.md)
- [Complete SDK development guide](docs/development-guide.md)
- [Per-interface SDK examples](docs/interface-examples.md)
- [完整 SDK 开发手册](docs/development-guide.zh-CN.md)
- [逐接口 SDK 示例](docs/interface-examples.zh-CN.md)
- [Installation guide](docs/installation.md)
- [SDK usage guide](docs/usage.md)
- [安装指南](docs/installation.zh-CN.md)
- [SDK 使用指南](docs/usage.zh-CN.md)
- [Example guide](examples/README.md)
