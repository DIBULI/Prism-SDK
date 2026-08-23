# Prism Host SDK 1.0.0 release manifest

This repository is the public binary distribution of Prism Host SDK 1.0.0.
No SDK implementation source code is included.

## Interface compatibility

- Host SDK: 1.0.0
- Device Agent: 1.0.0
- USB protocol: 1
- Runtime API: 5
- Public headers: 12 C++17 headers under `include/prism/`

The published headers and all four platform dynamic libraries have been verified as one
compatible 1.0.0 ABI set. Do not mix them with another SDK release.

## Linux x86-64

- Environment: Ubuntu 22.04, GCC 11.4, Release
- Runtime: `runtime/linux-x64/libprism_usb_sdk.so`
- SHA-256:
  `92dca1e5b10f300542cfb4396a4d8e5176fc8324f2d12419eb1788c25007aba2`
- ELF Build ID: `c144c08332b841b0d2c98355ccfe540cf95d651b`
- Maximum required symbol versions: GLIBC 2.34, GLIBCXX 3.4.29,
  OPENSSL 3.0.0
- Static archive: `runtime/linux-x64/libprism_usb_sdk.a`
- Static SHA-256:
  `20c1cb6fe21cc5adb5bd59390b085c5aecec9a5eabfb0ffdc9aea1ef581cee18`

## Linux arm64

- Environment: Ubuntu 22.04 cross toolchain, GCC 11.4, Release
- Runtime: `runtime/linux-arm64/libprism_usb_sdk.so`
- SHA-256:
  `f876944a639b4aa0200140de7b02ec618f6b74d04d1a8a69ab18d2189328a4c8`
- ELF Build ID: `a4a9977d36f6a103a81bd4429e19c727f9b28d07`
- Runtime dependencies: OpenSSL 3 and libusb 1.0
- ABI baseline: GLIBC 2.34 and GLIBCXX 3.4.29
- Static archive: `runtime/linux-arm64/libprism_usb_sdk.a`
- Static SHA-256:
  `52b681595459ac9f4bfddede7c8c217917954c3d74adfb106e82b64cce5a0394`

## ROS Adapter Linux prefixes

The SDK repository includes four complete binary installation prefixes under
`runtime/ros`. They contain only the public headers, dynamic library, CMake
package metadata, and udev rule. The ROS Adapter selects the prefix matching
its Ubuntu base image; libraries from different rows must not be interchanged.

| Prefix | Build environment | Runtime SHA-256 |
| --- | --- | --- |
| `ubuntu-20.04-x86_64` | Ubuntu 20.04, GCC 9, OpenSSL 1.1 | `1ec8c54e70abe46fa097066305943b7facd46c10e28a2d87e0573aa47333a193` |
| `ubuntu-22.04-x86_64` | Ubuntu 22.04, GCC 11, OpenSSL 3 | `92dca1e5b10f300542cfb4396a4d8e5176fc8324f2d12419eb1788c25007aba2` |
| `ubuntu-24.04-x86_64` | Ubuntu 24.04, GCC 13, OpenSSL 3 | `f8413a8d3c216c18c1915e116cf9102d9259ea6386cef2824c5a8e1ac3a4e752` |
| `ubuntu-26.04-x86_64` | Ubuntu 26.04, GCC 15, OpenSSL 3.5 | `6122e67d1c1a6f32f1dd11bd11cb36938a4bda9f8232da42e14f817db82f6725` |

Each prefix was built from the same SDK 1.0.0 interface and passed the complete
six-test Host SDK suite in its target container.

## macOS arm64

- Environment: Apple Clang, Release
- Minimum deployment target: macOS 13.0
- Runtime: `runtime/macos-arm64/libprism_usb_sdk.dylib`
- Runtime SHA-256:
  `35afe9431db86ab8c7f4c73703b46fbeda08eb009007ee0070fdbfee0587c987`
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
  `58f5636e23cfe5d399df219e064d2716a5d45049eab7da91f84993a01e35c51d`
- Linker toolchain: MSVC 14.44

The Windows package intentionally contains only the dynamic library. Consumers
load Runtime API v5 with `LoadLibraryW` and `GetProcAddress`, as demonstrated by
the included example.

## Release verification

- Linux and macOS runtimes report SDK 1.0.0 and Runtime API v5.
- Runtime API v5 is accepted; earlier API versions are rejected.
- Public headers compile and link against the frozen Linux and macOS runtimes.
- Linux x86-64 and arm64 static archives build all examples without a
  `libprism_usb_sdk.so` dependency and pass the six-test Host SDK suite.
- The Windows runtime completed its Host SDK test suite before publication.
