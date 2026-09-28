# Getting started: install, connect and read

[Documentation index](../README.md) · [简体中文](README.zh-CN.md)

<!-- page-toc -->
- [Installation and linking](#installation-and-linking)
  - [Linux](#linux-x86-64-and-arm64) · [macOS](#macos-arm64) · [Windows](#windows-x64)
  - [Package integrity](#integrity-verification)
- [First connection and reads](#first-connection-and-reads)
  - [Client lifecycle](#basic-lifecycle-on-linux-and-macos)
  - [Time synchronization](#time-synchronization) · [Threading](#threading-and-exclusive-access)
<!-- /page-toc -->

<a id="installation"></a>

## Installation and linking

### Supported binary targets

| Host platform | Architecture | Minimum environment | Libraries |
| --- | --- | --- | --- |
| Linux | x86-64 | Ubuntu 20.04 or later | `runtime/linux-x64/libprism_usb_sdk.so` / `.a` |
| Linux | arm64 | Ubuntu 20.04 or later | `runtime/linux-arm64/libprism_usb_sdk.so` / `.a` |
| macOS | arm64 | macOS 13.0 | `runtime/macos-arm64/libprism_usb_sdk.dylib` |
| Windows | x86-64 | Windows 10/11 | `runtime/windows-x64/prism_usb_sdk.dll` |

Both Linux shared runtimes are built on Ubuntu 20.04/GCC 9, embed OpenSSL,
and dynamically link libusb and the system C/C++ runtime. See ORIGIN.md for
build provenance. ARM64 RK-local embeds miniz/libcrypto and needs pthreads/dl and the C++ runtime;
no libusb or dynamic OpenSSL is required.

### Linux x86-64 and arm64

Install build tools and runtime dependencies:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libusb-1.0-0
```

Ubuntu 20.04 ships CMake 3.16; the examples need 3.20 or later. For example,
install Python pip and use `python3 -m pip install --user -i
https://pypi.tuna.tsinghua.edu.cn/simple cmake==3.31.6`, then add
`$HOME/.local/bin` to PATH. CI tests use CMake 3.31.6 on Ubuntu 20.04.

Neither shared Host runtime requires a dynamic OpenSSL library.

Install a udev rule so non-root applications can open VID:PID `2207:1201`:

```bash
sudo tee /etc/udev/rules.d/99-prism-usb.rules >/dev/null <<'RULE'
SUBSYSTEM=="usb", ATTR{idVendor}=="2207", ATTR{idProduct}=="1201", MODE="0660", GROUP="plugdev"
RULE
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Add the user to the selected group if needed, sign in again, and reconnect the
USB cable. Only one application may own the Prism USB interface at a time.

For development, either place `libprism_usb_sdk.so` next to an executable with
an `$ORIGIN` RPATH, or add its directory to the loader search path. The included
CMake example uses the application-private library approach.

To link the Prism SDK implementation statically, install `libusb-1.0-0-dev`
and `libssl-dev`, then configure the included example or your consumer with:

```bash
cmake -S . -B build-static -DPRISM_SDK_USE_STATIC=ON
cmake --build build-static --config Release
```

The resulting executable does not depend on `libprism_usb_sdk.so`. OpenSSL and
libusb remain dynamically linked unless the consumer deliberately selects
compatible static builds of those dependencies.

### macOS arm64

Install Xcode Command Line Tools and CMake before building the example:

```bash
xcode-select --install
brew install cmake
```

Homebrew is used here only to install CMake; the packaged SDK libraries do not
contain Homebrew load paths.

Keep these two files together:

```text
libprism_usb_sdk.dylib
libusb-1.0.0.dylib
```

Both use relocatable `@rpath` install names. For an application bundle, copy
them to `Contents/Frameworks`, add `@executable_path/../Frameworks` to the
executable RPATH, then sign the complete application. Include
`libusb-COPYING.txt` when redistributing the bundled libusb library.

If enumeration succeeds but opening the device is denied, close any Viewer or
command-line program already using the Prism USB interface and reconnect it.

### Windows x64

Use the Visual Studio 2022 C++ x64 toolchain and CMake. The public C++ ABI
requires MSVC 14.x with the matching SDK 1.2.0 headers; MinGW is not supported.
Install the current Microsoft Visual C++ 2015-2022 x64 Redistributable for
deployment, and bind the Prism USB interface to the Windows WinUSB driver.
Place `prism_usb_sdk.dll` beside the executable.

This package intentionally ships only the DLL, not an import library. Windows
applications should load `prism_usb_sdk.dll` with `LoadLibraryW`, resolve
`prism_usb_sdk_get_runtime_api`, and validate Runtime API version 18 before use.
The included example implements this pattern and always uses the DLL-compatible
`/MD` runtime and release iterator ABI, including when a Debug configuration is
selected.

### Integrity verification

From the repository root:

```bash
sha256sum --check SHA256SUMS
```

On macOS, use `shasum -a 256 -c SHA256SUMS`.

<a id="first-connection"></a>

## First connection and reads

This page is the short getting-started path. For every public control,
streaming, parsing, update, and Windows Runtime API entry, see the
[complete SDK development guide](../reference/host.md).

### Basic lifecycle on Linux and macOS

The direct `prism::Client` API shown in this section is linkable with the Linux
and macOS runtimes. The Windows package intentionally has no import library;
Windows applications must use Runtime API v18 as shown in
`examples/device_info_time_sync.cpp`.

Include the umbrella header:

```cpp
#include <prism/usb_sdk.hpp>
```

Discover, open, and inspect a device:

```cpp
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("no Prism device found");
}

auto client = prism::Client::open(devices.front());
const auto hello = client.hello();
const auto versions = client.deviceVersions();
const auto info = client.deviceInfo();
```

Select an enumerated device by `DeviceInfo::serial_number` when more than one is
connected. USB paths are not stable identities. `product_serial` is populated
only by `client.deviceInfo()` after opening the device.

Before acquisition, check at least:

- `info.usb3_connected` for Camera transport;
- `info.sensor_board_online`;
- `info.sensor_board_time_synced` when synchronized sensor timestamps are
  required;
- `info.camera_present_mask` and `info.imu_present_mask`;
- `info.imu_init_error_mask == 0`;
- `info.sensor_board_error_flags == 0`.

Do not assume that every unit contains two onboard IMUs; use the detected masks
and counts.

### Time synchronization

On Linux and macOS, `synchronizeTimeNtpLike()` measures the device-minus-host
clock offset without changing either clock:

```cpp
const auto measurement = client.synchronizeTimeNtpLike();
```

This measurement is also idle-only: Camera, onboard IMU, and LiDAR transfers
must all be stopped. Runtime API v18 does not expose this measurement-only call
to Windows consumers.

`synchronizeSystemTime()` makes the host wall clock authoritative for the
device and verifies the result:

```cpp
const auto result = client.synchronizeSystemTime();
```

A successful return is already verified. Verification failure is reported as
an exception. Windows performs the same operation through
`RuntimeApi::synchronize_system_time`, as demonstrated by the example.

Before setting time:

1. ensure the host UTC clock is correct;
2. stop Camera, onboard IMU, and LiDAR transfers;
3. call `synchronizeSystemTime()`;
4. check `verified`, the residual offset, and clock-status fields;
5. restart acquisition after the operation completes.

Sensor Board remains the master; host UTC is submitted through the Agent to
Sensor Board, then RK follows Sensor Board PPS/NMEA and feeds Ethernet PTP.
The Agent rejects host time-setting while external GNSS is synchronized.
No RTC is required. Never step device time during a recording.

### Threading and exclusive access

- Only one process may own the device USB interface.
- Use one thread as the `Client` I/O owner.
- Do not call `readFrame()` concurrently from multiple threads.
- Keep Camera decoding, point-cloud rendering, and disk I/O out of the USB
  receive path.
- Stop active streams before closing the client or changing idle-only settings.

### Errors

Public API failures are reported as C++ exceptions. Catch `std::exception` at
application boundaries and include its message in diagnostics. Version
mismatch, USB permissions, an already-open device, unplug events, and attempts
to set time while streaming are expected operational errors and should be shown
clearly to the user.

See the complete development guide above and the authoritative declarations in
the public headers under `include/prism/`.
