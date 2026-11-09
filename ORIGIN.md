# Prism SDK 1.3.0 package and compatibility

The distribution contains public headers, compiled libraries, examples and
documentation. Device firmware is distributed separately.

## Compatibility

- Host SDK and RK-local SDK: **1.3.0**. Host USB connections also support
  legacy Agent **1.2.0/1.2.1** HELLO profiles; RK-local remains version-matched.
- Current production Sensor Board firmware: **0.5.3**, with native RAW8 on 415RSV0.
  Other models require their own matching firmware; never cross-flash model packages.
  413GSV0 and 21315V0 naming support does not establish new hardware qualification.
- Runtime API: **19** (DeviceInfo model field). Network extension: **4** (plain TCP,
  token equals Web login password). Profile/live-exposure extension: **1**.
  Native camera-control, RTK-module control, raw dataset, LiDAR power and speed extensions: **1**.
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
The 2026-11-03 refresh rebuilds all four platform libraries from the same source
revision (recorded in each PROVENANCE.txt), with native RAW8 and canonical model
names 415RSV0, 413GSV0 and 21315V0. Wire model IDs are unchanged.
Headers and all platform libraries are updated together, including raw GNSS
reading and LiDAR speed control. Installing this SDK does not upgrade device
firmware or restart devices. Platform builds and package checks are separate
from physical-hardware validation.

The latest 2026-11-03 build also supports the current 415RSV0 image envelopes
used by Agent revision `0db22e6644c525906d1edc553e64fab5ca457974`.
Older RAW8 recordings remain readable. Rebuild Viewer and other consumers with
this matching package; an older 1.3.0 binary is not a substitute.

[Setup](docs/getting-started/README.md) · [API differences](docs/reference/rk-local.md)
