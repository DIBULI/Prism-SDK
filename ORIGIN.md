# Prism Host SDK 1.0.0 release manifest

This repository is the public binary distribution of Prism Host SDK 1.0.0.
No SDK implementation source code is included.

## Interface compatibility

- Host SDK: 1.0.0
- Device Agent: 1.0.0
- USB protocol: 1
- Runtime API: 5
- Public headers: 12 C++17 headers under `include/prism/`
- Onboard IMU: ICM45686 at a fixed 800 Hz

The published headers and all four platform dynamic libraries have been verified as one
compatible 1.0.0 ABI set. Do not mix them with another SDK release.

The runtimes were rebuilt from `DIBULI/Prism-agent` commit
`cc443541bfe71722ce6d49480761a52121c32146`. They retain the fixed 800 Hz
ICM45686 reporting and accept the Agent's `50..995000 us` exposure limits.
Linux builds and tests are recorded in Actions run `33150407098`; the Windows
build and tests are recorded in Actions run `33150407054`.

## Linux x86-64

- Environment: Ubuntu 22.04, GCC 11.4, Release
- Runtime: `runtime/linux-x64/libprism_usb_sdk.so`
- SHA-256:
  `73a0478da0c13dbeef26240b751d3debd027aa45701a4d723e629a293b175c28`
- ELF Build ID: `a7981f3b8d6a9bfda76b07516e9c88d982631728`
- Maximum required symbol versions: GLIBC 2.34, GLIBCXX 3.4.29,
  OPENSSL 3.0.0
- Static archive: `runtime/linux-x64/libprism_usb_sdk.a`
- Static SHA-256:
  `cc53bbd78e4481aabad83282023770b7ed7cf8bf5e6d98033130154c3cb41dfe`

## Linux arm64

- Environment: Ubuntu 22.04 cross toolchain, GCC 11.4, Release
- Runtime: `runtime/linux-arm64/libprism_usb_sdk.so`
- SHA-256:
  `86c702bdb40a307df590da6d4c5ed720923d75feb9d5a771b70514577e4d8e5a`
- ELF Build ID: `4b259fb06cf960c68c673000ba1f671c0747ba63`
- Runtime dependencies: OpenSSL 3 and libusb 1.0
- ABI baseline: GLIBC 2.34 and GLIBCXX 3.4.29
- Static archive: `runtime/linux-arm64/libprism_usb_sdk.a`
- Static SHA-256:
  `45dde324e5bff5285de24944741ae67b425a4b8d6f50a788200a78c0655086a8`

## ROS Adapter Linux prefixes

The SDK repository includes four complete binary installation prefixes under
`runtime/ros`. They contain only the public headers, dynamic library, CMake
package metadata, and udev rule. The ROS Adapter selects the prefix matching
its Ubuntu base image; libraries from different rows must not be interchanged.

| Prefix | Build environment | Runtime SHA-256 |
| --- | --- | --- |
| `ubuntu-20.04-x86_64` | Ubuntu 20.04, GCC 9, OpenSSL 1.1 | `917665c9e8204e3c117ab6e60ecdfff263f36e46ecd12d89b406f4f8cffe548f` |
| `ubuntu-22.04-x86_64` | Ubuntu 22.04, GCC 11, OpenSSL 3 | `73a0478da0c13dbeef26240b751d3debd027aa45701a4d723e629a293b175c28` |
| `ubuntu-24.04-x86_64` | Ubuntu 24.04, GCC 13, OpenSSL 3 | `bc09e37a26f23ee4ec69a0dd3dbcc103f731c94966691ed0b051422e1ae05cba` |
| `ubuntu-26.04-x86_64` | Ubuntu 26.04, GCC 15, OpenSSL 3.5 | `ec4a321203b630af3781cd01b0ac9f5eceb2668ce9573dc36d7671433087dc4d` |

Each prefix was built from the same SDK 1.0.0 interface and passed the complete
six-test Host SDK suite in its target container.

## macOS arm64

- Environment: Apple Clang, Release
- Minimum deployment target: macOS 13.0
- Runtime: `runtime/macos-arm64/libprism_usb_sdk.dylib`
- Runtime SHA-256:
  `070f33890c47d54189a0ac13726ed9ff4dceff798ea93090995950853ecd7de5`
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
  `5992b255f93c1cbef52bf227fc354ee6fe1f7621225fbc6d5d88a4981e3b8cb1`
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
- Every published runtime is rejected by package verification unless it
  contains the `50..995000 us` exposure validator and excludes the obsolete
  `200..995000 us` validator.
