# Prism SDK 1.2.0 provenance

This distribution contains public headers, compiled libraries, examples and
documentation, not SDK implementation sources or firmware.

## Source and compatibility

- Public headers and Linux/macOS builds: `DIBULI/Prism-agent@7f8bc7d4b09fe158ca24eef8740e1b778c634456`.
- Host/RK-local/Agent version 1.2.0; protocol 1; Runtime API 18.
- RTK-module control, raw dataset and LiDAR power extensions remain version 1.
- Matched Sensor Board 0.4.27 missing-camera firmware is required to continue
  acquisition when a physical camera input clock disappears.
- Replace headers/libraries together and rebuild consumers. Partial sets contain
  only complete JPEGs; missing slots stay empty. No stale image or time is invented.
- The header-only camera assembler requires periodic `expire()` calls on idle
  reads. RK-local manages partial-set expiry and ACK internally and is built PIC.

Host compiled implementation files, third-party sources, public ABI and runtime
exports are unchanged from `21c09ae94bbc66d3719c4a9cb684c9badeaec04a`.
The Windows DLL/import library therefore retain the previously qualified build
from that commit. This refresh adds header-only stream assembly and new consumer
behavior, not a Windows DLL ABI change. Windows binaries were not rebuilt in this
refresh: GitHub Actions jobs were blocked before execution by account billing.
Do not interpret a refreshed tag as a new Windows test run.

## Build and verification

- Linux x64/ARM64: clean commit archives built and source-tested in Ubuntu 20.04
  containers with GCC 9. Host shared/static builds each passed 16 tests; RK-local
  ARM64 passed 8. The compatibility baseline is GLIBC 2.31. Host shared libraries
  embed OpenSSL with hidden symbols and retain dynamic libusb dependencies.
- macOS ARM64: local AppleClang build, deployment target 13.0, bundled libusb
  1.0.30; 15 source tests passed. No Intel package. macOS 13 is a compatibility
  target, not the current hardware test host.
- Windows x64: retained MSVC `/MD` DLL/import library from the previously
  successful [distribution build](https://github.com/DIBULI/Prism-agent/actions/runs/36532500195).
- The consumer package includes a header-only GCC 9/11 optional-parser false
  positive workaround in `gnss_plot.hpp`, consistently applied to all installed
  headers. It changes no parsing rules, public interface or binary ABI.

`runtime/ros/linux-x64` is a shared Host SDK prefix; the ARM64 prefix is static.
RK-local's ARM64 archive embeds miniz/OpenSSL and requires pthreads/dl and system
C/C++ runtimes. Do not link Host and RK-local static libraries into one program;
they share codec symbols. No ROS node binaries are included.

Per-platform `PROVENANCE.txt` identifies each binary's actual source. `SHA256SUMS`
covers published files; Release archives have separate checksums. Device firmware,
recordings and hardware test reports are not bundled. Installing this SDK does
not update an Agent, Sensor Board, Viewer, ROS installation or SD image.

The corresponding Agent/image runs the hotspot independently of Agent lifecycle;
Wi-Fi APIs are unchanged. Migrating an old image requires its new helper/service
files, not just the Agent executable. Existing clock calibration validation and
keepalive fixes remain; UTC retention and later clock rollback are outside this
stream-resilience change.
