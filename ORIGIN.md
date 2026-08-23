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
  `73c8278aca4a50057d2fc0f897ac465a402bcbae2426a8f4c5b9eaa0ee5b2516`
- ELF Build ID: `8a12174f44dfd416573f93495c66ba7ff293257b`
- Maximum required symbol versions: GLIBC 2.34, GLIBCXX 3.4.29,
  OPENSSL 3.0.0
- Static archive: `runtime/linux-x64/libprism_usb_sdk.a`
- Static SHA-256:
  `3291bc77409be3926850178b7851cff955284cd5d0d3f315840caffcceae5451`

## Linux arm64

- Environment: Ubuntu 22.04 cross toolchain, GCC 11.4, Release
- Runtime: `runtime/linux-arm64/libprism_usb_sdk.so`
- SHA-256:
  `0c76621dcaa0a049b16db2c6790be8d766b312911fc8797cf300d441c60287e9`
- ELF Build ID: `d7f464a2b567620e943f3f8bd1da91ab0e9137a8`
- Runtime dependencies: OpenSSL 3 and libusb 1.0
- ABI baseline: GLIBC 2.34 and GLIBCXX 3.4.29
- Static archive: `runtime/linux-arm64/libprism_usb_sdk.a`
- Static SHA-256:
  `65d1c9d8307b15a956b5e7d47ea12b1cd872492d9db9773283696702857de4e2`

## ROS Adapter Linux prefixes

The SDK repository includes four complete binary installation prefixes under
`runtime/ros`. They contain only the public headers, dynamic library, CMake
package metadata, and udev rule. The ROS Adapter selects the prefix matching
its Ubuntu base image; libraries from different rows must not be interchanged.

| Prefix | Build environment | Runtime SHA-256 |
| --- | --- | --- |
| `ubuntu-20.04-x86_64` | Ubuntu 20.04, GCC 9, OpenSSL 1.1 | `03710c82755b6b2cdb924c425e85036f3a1cc751b841bd0680c9446a1e7888c1` |
| `ubuntu-22.04-x86_64` | Ubuntu 22.04, GCC 11, OpenSSL 3 | `73c8278aca4a50057d2fc0f897ac465a402bcbae2426a8f4c5b9eaa0ee5b2516` |
| `ubuntu-24.04-x86_64` | Ubuntu 24.04, GCC 13, OpenSSL 3 | `a58f369676f6b58d615d09ab3e9ebbac2c5c4f398dfd6dcc66a87a44f2f56f4f` |
| `ubuntu-26.04-x86_64` | Ubuntu 26.04, GCC 15, OpenSSL 3.5 | `518976542e7862c2cce540e7d5692d174c87021a18b32ef04b8c7a9e847619d1` |

Each prefix was built from the same SDK 1.0.0 interface and passed the complete
six-test Host SDK suite in its target container.

## macOS arm64

- Environment: Apple Clang, Release
- Minimum deployment target: macOS 13.0
- Runtime: `runtime/macos-arm64/libprism_usb_sdk.dylib`
- Runtime SHA-256:
  `03db8a8ff801f2623046818e2703b817308c4a8276b32dbcabbb1fb1664d93a4`
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
  `952681071ea9f8e17162927dbc12b06e4884b0a9636417e35c506a41ec05a8eb`
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
