# Prism Host SDK 0.11.0 release manifest

This repository is the public binary distribution of Prism Host SDK 0.11.0.
No SDK implementation source code is included.

## Interface compatibility

- Host SDK: 0.11.0
- Device Agent: 0.11.0
- USB protocol: 10
- Runtime API: 4
- Public headers: 12 C++17 headers under `include/prism/`

The published headers and all three dynamic libraries have been verified as one
compatible 0.11.0 ABI set. Do not mix them with another SDK release.

## Linux x86-64

- Environment: Ubuntu 24.04, GCC 13.3, Release
- Runtime: `runtime/linux-x64/libprism_usb_sdk.so`
- SHA-256:
  `b03d1dc616e5a489f519fd7816805ef985542329019c05b517c01dc0cbdbe9d0`
- ELF Build ID: `769ae2a92952ecc5f7c8b7c5d738ee8b27ec69ee`
- Maximum required symbol versions: GLIBC 2.38, GLIBCXX 3.4.29,
  OPENSSL 3.0.0

## macOS arm64

- Environment: Apple Clang, Release
- Minimum deployment target: macOS 13.0
- Runtime: `runtime/macos-arm64/libprism_usb_sdk.dylib`
- Runtime SHA-256:
  `6754ded41584578f8e62719f139c5a3c4115a7bc0a032738763218265e6034c1`
- Bundled dependency: `runtime/macos-arm64/libusb-1.0.0.dylib`
- libusb SHA-256:
  `6f65716831f5072bbae4286903c1efce7588ecdbf9d9d4df01122a30cded3b01`

Both dylibs are arm64 Mach-O files with relocatable install names and no
package-build-machine load paths. The bundled libusb is version 1.0.30; its
license text is included beside the dylib.

## Windows x64

- Environment: Windows Server 2022, MSVC x64, Release
- Runtime: `runtime/windows-x64/prism_usb_sdk.dll`
- SHA-256:
  `e252a51b54fb03c4615ca54d14d7b217c810f99933518e6e83102e82de2df7f6`
- Linker toolchain: MSVC 14.44

The Windows package intentionally contains only the dynamic library. Consumers
load Runtime API v4 with `LoadLibraryW` and `GetProcAddress`, as demonstrated by
the included example.

## Release verification

- Linux and macOS runtimes report SDK 0.11.0 and Runtime API v4.
- Runtime API v4 is accepted; earlier API versions are rejected.
- Public headers compile and link against the frozen Linux and macOS runtimes.
- The Windows runtime completed its Host SDK test suite before publication.
