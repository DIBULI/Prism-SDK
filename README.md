# Prism SDK 1.2.0

[简体中文](README.zh-CN.md) · [API documentation](docs/README.md)

Binary distribution of the Host SDK and RK-local C++17 SDK. Includes public
headers, compiled libraries, consumer examples and documentation; no SDK
implementation or device firmware source is published here.

## Compatibility

- SDK / required Agent: **1.2.0**; opening enforces an exact version match.
- Qualified Sensor Board: **0.4.27**; USB and RK-local protocol **1**.
- Windows Runtime API **18**, RTK-module control extension **1**.
- Replace headers and libraries together and rebuild your application.

| Platform | Published libraries | Baseline |
| --- | --- | --- |
| Linux x64 / ARM64 | Host `.so` and `.a` | Ubuntu 20.04 ABI; tested by CI on 20.04/22.04/24.04/26.04 |
| RK3576 Linux ARM64 | `libprism_rklocal_sdk.a` | RK-local C++ Client |
| macOS Apple Silicon | SDK + bundled libusb `.dylib` | macOS 13+; no Intel package |
| Windows x64 | SDK DLL via Runtime API | MSVC 14.x, `/MD`, Windows 10/11 target |

Linux shared libraries embed OpenSSL and depend on system libusb-1.0. Static
Host linking additionally needs libusb and OpenSSL development libraries.
RK-local embeds its archive dependencies and needs pthreads/dl, not libusb.
`runtime/ros/linux-x64` and `runtime/ros/linux-arm64` are matching installed
Host SDK prefixes, not ROS adapter binaries or Docker images.

## What's included

- Unified Host/RK-local CORS configuration and RTK start/stop APIs.
- Current TimeSync modes, RTK-module firmware versions, 4G/control diagnostics.
- Receiver-native GNSS/RTK observations, sky/trajectory display helpers; no
  Agent-side RTK solver or retired raw/smoothed navigation interface.
- GNSS input diagnostics, XT32 support, LiDAR line fields and camera metadata.
- [Release notes](docs/update/v1.2.0.md), [provenance](ORIGIN.md) and `SHA256SUMS`.

## Build consumer examples

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

On Ubuntu/Debian install `libusb-1.0-0-dev`; static Host consumers also need
`libssl-dev` and `-DPRISM_SDK_USE_STATIC=ON`. Install the USB permission rule once:

```sh
sudo install -m 0644 runtime/ros/linux-x64/lib/udev/rules.d/99-prism-usb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

The same rule applies to ARM64. Replug USB if needed. Windows examples load
the adjacent DLL; do not directly link the Linux/macOS Client examples on Windows.
macOS examples use the two bundled dylibs without a Homebrew runtime dependency.

For RK-local build `cmake -S rk-local-sdk -B build/rklocal` on the RK, or use
the supplied ARM64 toolchain; see [RK-local API and differences](docs/rk-local-sdk.md).

Opening a client does not set time, start acquisition or start CORS. CORS save
and RTK start are separate actions; starting requires explicit consent to send
live GGA. Use `prism-rtk-module-control --help` for the Host example and
`prism-rklocal-rtk-module-control --help` from the top-level ARM64 build.

All usage guides are in [docs/](docs/README.md). Full package checks:
`python3 scripts/test_all_examples.py --build-dir build-all-examples`.
