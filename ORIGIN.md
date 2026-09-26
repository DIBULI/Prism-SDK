# Prism SDK 1.2.0 provenance

This repository contains only public headers, compiled SDK libraries, consumer
examples and documentation. No SDK implementation or device firmware is included.

## Matched source

- Host / RK-local / required Agent version: `1.2.0`.
- Qualified Sensor Board: `0.4.27`; USB / RK-local protocol: `1`.
- Runtime API: `18`; RTK-module control extension: `1`.
- Source repository: `DIBULI/Prism-agent`.
- Immutable source commit: `d10fdcfc7919c082ad974bb79487d9c3c02cbee9`.
- Build: [matched SDK distribution run 36237563907](https://github.com/DIBULI/Prism-agent/actions/runs/36237563907).
- All four platform jobs passed their source tests before packaging.

All installed Host headers are identical across platforms after normalizing
Windows CRLF to LF. RK-local uses the same Host types and adds its own C++ Client.
Consumers must replace headers and libraries together and rebuild.

Packaging applies a header-only GCC 9/11 compatibility adjustment to
`gnss_plot.hpp`: initialize the optional numeric-parser payload before parsing
and preserve the parser call boundary on older GCC to avoid a false positive
about disengaged optional storage after inlining.
It does not change the public interface, parsing rules or binary ABI. The same
adjustment is present in both installed ROS header prefixes and is covered by
the numeric-parser consumer self-test. Compiled libraries remain unmodified
artifacts of the source commit above.

## Platforms

- Linux x64 / ARM64: Ubuntu 20.04 / GCC 9 ABI baseline. Host shared libraries
  embed OpenSSL with its symbols hidden; libusb remains a dynamic dependency.
  Both shared libraries require at most GLIBC 2.25 and GLIBCXX 3.4.22.
  Host static archives require target libusb/OpenSSL development dependencies.
- RK-local ARM64: static C++17 archive with embedded miniz/OpenSSL; pthreads/dl
  and system C/C++ runtimes are required. Do not link both Host and RK-local
  static archives into the same executable because they share codec symbols.
- macOS ARM64: deployment target 13.0, bundled libusb 1.0.30, relocatable
  dylibs built on macOS 15. No Intel package.
- Windows x64: MSVC on Windows Server 2022, `/MD`, Runtime API DLL loading.
  Windows 10/11 and macOS 13 are compatibility targets, not hardware test hosts.

`runtime/ros/linux-x64` is an installed shared Host prefix;
`runtime/ros/linux-arm64` is an installed static Host prefix. No ROS adapter
binary or Docker image is released here.

Per-platform provenance is in `runtime/*/PROVENANCE.txt`. `SHA256SUMS` covers
published files; external Release archives have separate SHA-256 checksums.
Consumer-package CI tests loading, example builds, and Linux compatibility
independently of the source build. No new device firmware is flashed by installing
this SDK, and no device recordings or hardware test reports are included.
