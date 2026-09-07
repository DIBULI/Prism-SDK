# Prism SDK 1.1.0 release provenance

This binary-distribution repository contains public headers, prebuilt libraries,
consumer examples and documentation. It does not publish SDK implementation or
firmware source code.

## Source and interface baseline

- Distribution / Host SDK / RK-local SDK: **1.1.0**
- Required device Agent: **exactly 1.1.0** (no old-Agent fallback)
- Qualified Sensor Board: **0.4.26**
- Host USB protocol / RK-local protocol: **1 / 1**
- Windows Runtime API: **12**, 57 function pointers, MSVC C++ ABI
- Runtime source: `DIBULI/Prism-agent` commit
  `e47627fab53946c854bbdb0f83fee9a0a209d98e`
- Host source directory: `prism-sdk/usb-sdk`
- RK-local source directory: `prism-rklocal-sdk`
- Runtime build: [Agent Actions run 34081177379](https://github.com/DIBULI/Prism-agent/actions/runs/34081177379)
- Build workflow revision: `1d437fd`, `.github/workflows/build-sdk-distribution.yml`

The build runs inside the private source repository; a public SDK repository
token cannot check out the private implementation. Published artifacts contain
only the installed SDK. Header comments clarify the current 800 Hz, GNSS and
Sensor Board time semantics; declarations and binary layouts match the baseline.
Examples in this package are consumer examples, compiled against these binaries.

## Linux x64 and ARM64

Both architectures use Ubuntu 20.04/GCC 9. Shared runtimes embed Ubuntu
OpenSSL `1.1.1f-1ubuntu2.24`, with archive symbols hidden using
`-Wl,--exclude-libs,ALL`. They have no dynamic libcrypto/libssl dependency;
libusb and system C/C++ runtime libraries remain dynamic. Maximum required
GLIBC is 2.25 and GLIBCXX is 3.4.22 on both architectures.

`runtime/linux-{x64,arm64}` includes both Host `.so` and `.a`. Static Host
archives do not embed their dependencies: consumers resolve libusb, OpenSSL
and threads using target-system development packages. ARM64 also includes
`libprism_rklocal_sdk.a`; this C SDK only needs pthreads/system C libraries.

`runtime/ros/linux-x64` is the complete shared installed prefix;
`runtime/ros/linux-arm64` is the complete static installed prefix. Both carry
1.1.0 headers, exact-version CMake package metadata and udev rules. These are
SDK installation prefixes; no ROS adapter implementation is included or changed.

## macOS and Windows

- macOS: Apple Silicon ARM64, deployment target 13.0; SDK dylib and bundled
  libusb 1.0.30 rebuilt on macOS 15, relocatable and ad-hoc signed.
- Windows: x64 MSVC on Windows Server 2022; DLL intended for Windows 10/11,
  loaded through Runtime API 12 with compatible MSVC 14.x and `/MD`.
  The distribution intentionally does not add a Windows import library.

Windows 10/11 and macOS 13 are compatibility targets, not physical-device test
environments. CI executes on Windows Server 2022/macOS 15. Linux compatibility
CI tests Ubuntu 20.04/22.04/24.04/26.04 for both architectures.

## Integrity and verification

All four runtime-build jobs passed. Linux shared/static builds each passed
11 Host tests; ARM64 RK-local passed the mock-Agent test. macOS/Windows also
passed the source SDK suites. Package CI separately compiles consumer examples,
checks shared/static loading, and verifies every published file in `SHA256SUMS`.
Regenerate the manifest with `bash scripts/update_checksums.sh` after staging
package edits. Never mix headers or libraries from another SDK release.
