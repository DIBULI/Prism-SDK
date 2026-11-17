# Prism SDK 1.4.0 package and compatibility

The distribution contains public headers, compiled libraries, examples and
documentation. Device firmware is distributed separately.

## Compatibility

- Host SDK and RK-local SDK: **1.4.0**, compatible with Agent **1.3.0/1.3.1/1.4.0**.
  Host connections also support legacy Agent **1.2.0/1.2.1** HELLO profiles.
  Unknown protocol versions remain rejected; legacy devices retain their feature limits.
- Current matching Sensor Board firmware: **0.5.4**, with native RAW8 on 415RSV0 and 21312V0.
  Other models require their own matching firmware; never cross-flash model packages.
  413GSV0 and 21312V0 naming support does not establish new hardware qualification.
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
The 1.4.0 update rebuilds all four platform libraries from the same source
revision (recorded in each PROVENANCE.txt), with native RAW8 and canonical model
names 415RSV0, 413GSV0 and 21312V0. Wire model IDs are unchanged.
Headers and all platform libraries are updated together, including raw GNSS
reading and LiDAR speed control. Installing this SDK does not upgrade device
firmware or restart devices. Platform builds and package checks are separate
from physical-hardware validation.

The update retains earlier 415RSV0 image-envelope support and adds 21312V0
4096×3072 RAW8 decoding. Rebuild Viewer and other consumers with matching headers
and libraries; renaming an older SDK binary does not upgrade it.

## Heading interface update (2026-11-17)

`gnss_heading.hpp` and the shared GNSS display model add header-only parsing
of UNIHEADINGA through the existing GNSS observation API. This additive update
uses the 1.4.0 runtime libraries already in this package; their recorded build
provenance remains unchanged. No transport command, exported symbol or runtime
table changes. Rebuild consumers with the updated headers.

The Agent must include the heading observation patch from source revision
`66235007f241c34eb0848dc94bb06d0c5803f82b`; older Agents may omit heading
observations even when raw GNSS forwarding works. Live Agent reporting was
verified on 413GSV0 with RTK Module 1.0.3 and Sensor Board 0.5.3. This establishes
transport and parser behavior, not antenna calibration or fixed-heading accuracy.

[Setup](docs/getting-started/README.md) · [API differences](docs/reference/rk-local.md)
