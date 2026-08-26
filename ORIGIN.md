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

## Linux x86-64

- Environment: Ubuntu 22.04, GCC 11.4, Release
- Runtime: `runtime/linux-x64/libprism_usb_sdk.so`
- SHA-256:
  `0eefa16ad23324c5a6bd3ba8263906e01ca6f1971ae59c96ec9f99ba6e99d244`
- ELF Build ID: `32489ce23e10d4be824c090657a772fb649a3cb5`
- Maximum required symbol versions: GLIBC 2.34, GLIBCXX 3.4.29,
  OPENSSL 3.0.0
- Static archive: `runtime/linux-x64/libprism_usb_sdk.a`
- Static SHA-256:
  `cfbb8c7ab1b1923cc60de24c10a5d13845c15949187c1610d0068a25d34fa669`

## Linux arm64

- Environment: Ubuntu 22.04 cross toolchain, GCC 11.4, Release
- Runtime: `runtime/linux-arm64/libprism_usb_sdk.so`
- SHA-256:
  `39e3c02db743af8193f5782fe5eb14aa1726675505a82b33c3af060e359ac841`
- ELF Build ID: `ddbf6927dfd345653974db6c40f3d4d9d2e56e2f`
- Runtime dependencies: OpenSSL 3 and libusb 1.0
- ABI baseline: GLIBC 2.34 and GLIBCXX 3.4.29
- Static archive: `runtime/linux-arm64/libprism_usb_sdk.a`
- Static SHA-256:
  `b6763de2c6c164e9b0e3f76f4ec6cdcc086d8d2a62aacfe3a74d6d3d60a893f9`

## ROS Adapter Linux prefixes

The SDK repository includes four complete binary installation prefixes under
`runtime/ros`. They contain only the public headers, dynamic library, CMake
package metadata, and udev rule. The ROS Adapter selects the prefix matching
its Ubuntu base image; libraries from different rows must not be interchanged.

| Prefix | Build environment | Runtime SHA-256 |
| --- | --- | --- |
| `ubuntu-20.04-x86_64` | Ubuntu 20.04, GCC 9, OpenSSL 1.1 | `f1d2174b3917d02b6b52ad581efd0d5ab9e2a27b5957779471e6b19f7051eefb` |
| `ubuntu-22.04-x86_64` | Ubuntu 22.04, GCC 11, OpenSSL 3 | `0eefa16ad23324c5a6bd3ba8263906e01ca6f1971ae59c96ec9f99ba6e99d244` |
| `ubuntu-24.04-x86_64` | Ubuntu 24.04, GCC 13, OpenSSL 3 | `a813bceb42f297569bbf4b043aa2b9b9430a34db32b91954bccd23f83b5c3e3a` |
| `ubuntu-26.04-x86_64` | Ubuntu 26.04, GCC 15, OpenSSL 3.5 | `8a6dcaa2cfa5c20155d83a83524b1768505a5adc77d981f4334c8cc3015803e5` |

Each prefix was built from the same SDK 1.0.0 interface and passed the complete
six-test Host SDK suite in its target container.

## macOS arm64

- Environment: Apple Clang, Release
- Minimum deployment target: macOS 13.0
- Runtime: `runtime/macos-arm64/libprism_usb_sdk.dylib`
- Runtime SHA-256:
  `615396aa3ea5f11e8bec8da6df7185d2e681e7e3b6b31a2a45622f15ad7e0551`
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
  `6ee2def2aaf702147303e1d4d2b20ba4ef0a0531951e4585fe4aa73c52268bf9`
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
