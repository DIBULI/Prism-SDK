# Prism Host SDK 1.0.0 release manifest

This repository is the public binary distribution of Prism Host SDK 1.0.0.
No SDK implementation source code is included.

## Interface compatibility

- Host SDK: 1.0.0
- Device Agent: 1.0.0
- USB protocol: 1
- Runtime API: 4
- Public headers: 12 C++17 headers under `include/prism/`

The published headers and all three dynamic libraries have been verified as one
compatible 1.0.0 ABI set. Do not mix them with another SDK release.

## Linux x86-64

- Environment: Ubuntu 22.04, GCC 11.4, Release
- Runtime: `runtime/linux-x64/libprism_usb_sdk.so`
- SHA-256:
  `d2558c446378268f4a46b43a305ee9128a9cf018c72d1a8f8b4334ec2171fed0`
- ELF Build ID: `fae7e625cd5d26f6f92c2e95c25268be7fb7f79a`
- Maximum required symbol versions: GLIBC 2.34, GLIBCXX 3.4.29,
  OPENSSL 3.0.0

## Linux arm64

- Environment: Ubuntu 22.04 cross toolchain, GCC 11.4, Release
- Runtime: `runtime/linux-arm64/libprism_usb_sdk.so`
- SHA-256:
  `c95bc2af28668294e58cf3aa620ecdde9fc30ea4e69b94a555f6123f1091da4b`
- ELF Build ID: `790d85a0965385eb98b77397f666479a3c7d4c61`
- Runtime dependencies: OpenSSL 3 and libusb 1.0
- ABI baseline: GLIBC 2.34 and GLIBCXX 3.4.29

## ROS Adapter Linux prefixes

The SDK repository includes four complete binary installation prefixes under
`runtime/ros`. They contain only the public headers, dynamic library, CMake
package metadata, and udev rule. The ROS Adapter selects the prefix matching
its Ubuntu base image; libraries from different rows must not be interchanged.

| Prefix | Build environment | Runtime SHA-256 |
| --- | --- | --- |
| `ubuntu-20.04-x86_64` | Ubuntu 20.04, GCC 9, OpenSSL 1.1 | `debe2a3a7416edcb2c9ceebd8bfb3a06ea3eea0eb57ceb544ac1fea14a375b27` |
| `ubuntu-22.04-x86_64` | Ubuntu 22.04, GCC 11, OpenSSL 3 | `d2558c446378268f4a46b43a305ee9128a9cf018c72d1a8f8b4334ec2171fed0` |
| `ubuntu-24.04-x86_64` | Ubuntu 24.04, GCC 13, OpenSSL 3 | `aed16bf46556477e1285a13ad56438be4f65bf280175d375c179349ea682a7d1` |
| `ubuntu-26.04-x86_64` | Ubuntu 26.04, GCC 15, OpenSSL 3.5 | `fb60b3cde585b37944df0c0df9b301a6cf2c89ecc45973321092eb856821d840` |

Each prefix was built from the same SDK 1.0.0 interface and passed the complete
six-test Host SDK suite in its target container.

## macOS arm64

- Environment: Apple Clang, Release
- Minimum deployment target: macOS 13.0
- Runtime: `runtime/macos-arm64/libprism_usb_sdk.dylib`
- Runtime SHA-256:
  `9317a80bf597933039c5fb7030448b65f1085e611cbc1b2482cc05de675c416c`
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
  `abe12bb66b63102237b691466a680aa45ecaa0442e68a55cc65a0057b6e1a87d`
- Linker toolchain: MSVC 14.44

The Windows package intentionally contains only the dynamic library. Consumers
load Runtime API v4 with `LoadLibraryW` and `GetProcAddress`, as demonstrated by
the included example.

## Release verification

- Linux and macOS runtimes report SDK 1.0.0 and Runtime API v4.
- Runtime API v4 is accepted; earlier API versions are rejected.
- Public headers compile and link against the frozen Linux and macOS runtimes.
- The Windows runtime completed its Host SDK test suite before publication.
