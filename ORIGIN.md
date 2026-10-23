# Prism SDK 1.3.0 package and compatibility

The distribution contains public headers, compiled libraries, examples and
documentation. Device firmware is distributed separately.

## Compatibility

- Host SDK, RK-local SDK and required Agent version: **1.3.0**.
- Matching Sensor Board firmware: **0.5.0**. Use the current firmware package
  for internally aligned IMU output and missing-camera/shared-exposure fixes.
- Runtime API: **19** (DeviceInfo model field). Network extension: **4** (plain TCP,
  token equals Web login password). Profile/live-exposure extension: **1**.
  RTK-module control, raw dataset, LiDAR power and speed extensions: **1**.
- Update headers and libraries together and rebuild consumers. A matching version
  number alone does not establish that an older device has every current feature.

## Platforms

- Linux x64/ARM64: glibc 2.31+ baseline. Shared Host libraries need libusb and
  system C/C++ runtimes; static-link dependencies are described in the setup guide.
- RK-local: Linux ARM64 static library with PIC; pthreads/dl and system C/C++
  runtimes are required. Do not link Host and RK-local static libraries together.
- macOS: Apple Silicon, macOS 13+, bundled libusb. No Intel package.
- Windows: x64/MSVC, Windows 10/11.

## Package verification

`SHA256SUMS` verifies the distributed files; Release archives also have download
checksums. Per-platform `PROVENANCE.txt` records public compatibility information.
Headers and all platform libraries are updated together, including raw GNSS
reading and LiDAR speed control. Installing this SDK does not upgrade device
firmware or restart devices. Platform builds and package checks are separate
from physical-hardware validation.

[Setup](docs/getting-started/README.md) · [API differences](docs/reference/rk-local.md)
