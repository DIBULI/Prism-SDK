# Installation

## Supported binary targets

| Host platform | Architecture | Minimum environment | Libraries |
| --- | --- | --- | --- |
| Linux | x86-64 | Ubuntu 20.04 or later | `runtime/linux-x64/libprism_usb_sdk.so` / `.a` |
| Linux | arm64 | Ubuntu 20.04 or later | `runtime/linux-arm64/libprism_usb_sdk.so` / `.a` |
| macOS | arm64 | macOS 13.0 | `runtime/macos-arm64/libprism_usb_sdk.dylib` |
| Windows | x86-64 | Windows 10/11 | `runtime/windows-x64/prism_usb_sdk.dll` |

Both Linux shared runtimes are built on Ubuntu 20.04/GCC 9, embed OpenSSL,
and dynamically link libusb and the system C/C++ runtime. See ORIGIN.md for
build provenance. ARM64 RK-local consumers need pthreads, not USB or OpenSSL.

## Linux x86-64 and arm64

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

## macOS arm64

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

## Windows x64

Use the Visual Studio 2022 C++ x64 toolchain and CMake. The public C++ ABI
requires MSVC 14.x with the matching SDK 1.1.0 headers; MinGW is not supported.
Install the current Microsoft Visual C++ 2015-2022 x64 Redistributable for
deployment, and bind the Prism USB interface to the Windows WinUSB driver.
Place `prism_usb_sdk.dll` beside the executable.

This package intentionally ships only the DLL, not an import library. Windows
applications should load `prism_usb_sdk.dll` with `LoadLibraryW`, resolve
`prism_usb_sdk_get_runtime_api`, and validate Runtime API version 12 before use.
The included example implements this pattern and always uses the DLL-compatible
`/MD` runtime and release iterator ABI, including when a Debug configuration is
selected.

## Integrity verification

From the repository root:

```bash
sha256sum --check SHA256SUMS
```

On macOS, use `shasum -a 256 -c SHA256SUMS`.
