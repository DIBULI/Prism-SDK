# Installation

## Supported binary targets

| Host platform | Architecture | Minimum environment | Runtime |
| --- | --- | --- | --- |
| Linux | x86-64 | Ubuntu 22.04 or later | `runtime/linux-x64/libprism_usb_sdk.so` |
| Linux | arm64 | Ubuntu 22.04 or later | `runtime/linux-arm64/libprism_usb_sdk.so` |
| macOS | arm64 | macOS 13.0 | `runtime/macos-arm64/libprism_usb_sdk.dylib` |
| Windows | x86-64 | Windows 10/11 | `runtime/windows-x64/prism_usb_sdk.dll` |

Both Linux runtimes require GLIBC 2.34 or later and OpenSSL 3. They are not
compatible with Ubuntu 20.04.

## Linux x86-64 and arm64

Install build tools and runtime dependencies:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libusb-1.0-0 libssl3
```

On Ubuntu 24.04, install `libssl3t64` instead of `libssl3`.

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
requires MSVC 14.x with the matching SDK 1.0.0 headers; MinGW is not supported.
Install the current Microsoft Visual C++ 2015-2022 x64 Redistributable for
deployment, and bind the Prism USB interface to the Windows WinUSB driver.
Place `prism_usb_sdk.dll` beside the executable.

This package intentionally ships only the DLL, not an import library. Windows
applications should load `prism_usb_sdk.dll` with `LoadLibraryW`, resolve
`prism_usb_sdk_get_runtime_api`, and validate Runtime API version 4 before use.
The included example implements this pattern and always uses the DLL-compatible
`/MD` runtime and release iterator ABI, including when a Debug configuration is
selected.

## Integrity verification

From the repository root:

```bash
sha256sum --check SHA256SUMS
```

On macOS, use `shasum -a 256 -c SHA256SUMS`.
