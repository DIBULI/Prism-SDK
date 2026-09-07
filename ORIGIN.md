# Prism SDK 1.1.0 release provenance

This binary-distribution repository contains public headers, prebuilt libraries,
consumer examples and documentation. It does not publish SDK implementation or
firmware source code.

## RK-local C++ update (2026-09-07)

Only the RK-local archive and its public API/examples are rebuilt for this update.
The Host binaries and protocol versions remain unchanged. RK-local now exposes
`prism::rklocal::Client` with shared Host controls for configuration, exposure,
LiDAR, Wi-Fi, GNSS/RTK, time, upgrades and raw RTCM; the C header is private.
[Explicit differences](docs/rk-local-sdk.md#host-api-alignment-and-explicit-differences)
and [new validation results](docs/rk-local-sdk-testing.md) define the supported scope.
Source: the RK-local C++ changes on top of the Agent baseline below, not yet
published as a new source commit/tag. Final server build: Ubuntu 20.04 ARM64/GCC 9.4.0 container;
control/upgrade test requires at most GLIBC 2.17 and GLIBCXX 3.4.21.
Consumers must replace the header and archive together and rebuild.
Physical RK3576 testing passed 19 query/capture/stop/reconnect checks using a
temporary Agent write-lock isolation fix. USB/local coexistence requires this
Agent fix; see the hardware report for its exact binary hash and deployment.
The existing firmware image and Sensor Board BOOT.BIN are not updated by this SDK package.

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
`libprism_rklocal_sdk.a`; this C++17 SDK embeds miniz/OpenSSL libcrypto, and needs pthreads/dl and
system C/C++ libraries, not dynamic libcrypto/libssl.

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
