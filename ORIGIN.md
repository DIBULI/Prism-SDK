# Prism SDK 1.2.0 package and compatibility

The distribution contains public headers, compiled libraries, examples and
documentation. Device firmware is distributed separately.

## Compatibility

- Host SDK, RK-local SDK and required Agent version: **1.2.0**.
- Matching Sensor Board firmware: **0.4.27**. Use the current firmware package
  for internally aligned IMU output and missing-camera/shared-exposure fixes.
- Runtime API: **18**. RTK-module control, raw dataset and LiDAR power extensions: **1**.
- Update headers and libraries together and rebuild consumers. A matching version
  number alone does not establish that an older device has every current feature.

## Platforms

- Linux x64/ARM64: glibc 2.31+ baseline. Shared Host libraries need libusb and
  system C/C++ runtimes; static-link dependencies are described in the setup guide.
- RK-local: Linux ARM64 static library with PIC; pthreads/dl and system C/C++
  runtimes are required. Do not link Host and RK-local static libraries together.
- macOS: Apple Silicon, macOS 13+, bundled libusb. No Intel package.
- Windows: x64/MSVC, Windows 10/11. The compatible, previously validated DLL
  is retained; a documentation refresh does not imply that it was rebuilt.

## Package verification

`SHA256SUMS` verifies the distributed files; Release archives also have download
checksums. Per-platform `PROVENANCE.txt` records public compatibility information.
This documentation refresh leaves runtime libraries unchanged. It does not
upgrade firmware, restart devices or establish new physical-hardware validation.

[Setup](docs/getting-started/README.md) · [API differences](docs/reference/rk-local.md)
