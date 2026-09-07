# RK-local SDK

The RK-local SDK is the Linux C API for applications running directly on the
Prism RK3576. It connects to `prism-agent` through
`/run/prism/stream.sock`. The Agent remains the only owner of Sensor Board
UARTs, Camera CSI/V4L2 devices, time synchronization, and capture sessions.

## Package files

- Public header: `include/prism/rklocal_sdk.h`
- ARM64 static library: `runtime/linux-arm64/libprism_rklocal_sdk.a`
- CMake imported target: `cmake/PrismRkLocalSdk.cmake`
- Example: `examples/rklocal_capture.c`
- Read-only GNSS example: `examples/rklocal_gnss_status.c`

## Build and run on RK

```sh
cmake -S examples/rklocal -B build/rklocal -DCMAKE_BUILD_TYPE=Release
cmake --build build/rklocal --parallel
ctest --test-dir build/rklocal --output-on-failure
# Ten seconds, four cameras + IMU0, no image files:
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 - 1
# Optional: save the first four JPEGs in a NEW directory:
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 ./first-jpegs 1
# Ten read-only GNSS queries, 100 ms apart:
build/rklocal/prism-rklocal-gnss-status /run/prism/stream.sock 10
```

For cross-compilation add `-DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/aarch64-linux-gnu.cmake`
and, if needed, `-DPRISM_AARCH64_CROSS_PREFIX=/path/to/aarch64-none-linux-gnu-`.
Only C, pthreads and the packaged ARM64 archive are needed, not USB/OpenSSL.
The capture example defaults to one IMU and uses the Agent's configured IMU
rate (currently 800 Hz). It prints first IMU values, image dimensions,
timestamps/exposure, then totals. IMU units are mg and millidegrees/s;
timestamps are microseconds. Every image buffer is a complete JPEG.
Use IMU count 2 only if both IMUs are present. Ctrl-C stops capture. Empty
image/IMU output or a capture error produces a nonzero exit status.
The GNSS example never starts capture or sets time. A successful query without
live GNSS may report no fix/PPS invalid; this is not a transport error.

The SDK is version `1.1.0`, uses local protocol version `1`, and requires an
exact Agent `1.1.0` handshake. It is not a USB Host SDK and is not available
for Windows, macOS, or Linux x86-64.

## CMake integration

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_rk_app LANGUAGES C)

include("/opt/Prism-SDK/cmake/PrismRkLocalSdk.cmake")
add_executable(my_rk_app main.c)
target_link_libraries(my_rk_app PRIVATE Prism::RkLocal)
```

The imported target supplies the public include directory, static archive, and
pthread dependency. The final executable does not need a separate RK-local SDK
shared library.

## Connection and ownership

```c
prism_rklocal_config_t config;
prism_rklocal_client_t *client = NULL;

prism_rklocal_config_default(&config);
if (prism_rklocal_open(&client, &config) != PRISM_RKLOCAL_OK)
  return 1;
/* Use the client. */
prism_rklocal_close(client);
```

One local client may control capture at a time. The SDK maintains receive and
keepalive threads and bounded IMU/frame queues. A slow consumer drops the
oldest local data instead of blocking the Agent.

## Camera and IMU

`prism_rklocal_start()` starts aggregate Camera and onboard-IMU acquisition.
Set `imu_sensor_count` to `1` on a unit with only IMU0, otherwise use `2`.
Read IMU samples with `prism_rklocal_read_imu()` and complete four-camera JPEG
sets with `prism_rklocal_read_frame_set()`. Release every successful frame set
with `prism_rklocal_frame_set_release()`.

## GPS/GNSS and CORS

`prism_rklocal_get_gnss_status()` reports fix, position, satellites, DOP, NMEA
freshness, time-synchronized state, PPS validity, and PPS high width. It does
not expose private RK-PPS phase-quality internals.

`prism_rklocal_get_rtk_correction_status()` reports the active correction
source, detected RTCM2.x/RTCM3.x format, byte counters, observation epochs,
solutions, and decoder errors.

A local application may call the begin/send/end correction functions to pass
raw RTCM2.x or RTCM3.x bytes to the Agent. NTRIP endpoint selection,
authentication, reconnects, and GGA upload remain application responsibilities.

## RTK navigation

`prism_rklocal_get_rtk_navigation()` queries the current navigation snapshot;
`prism_rklocal_read_rtk_navigation()` waits for the newest live event. Each
result retains the raw RTKLIB position and separately reports the independent
dynamics-enabled smoothed position, transition/jump gate flags, and reset
counters. SINGLE fallback is not used for either result.

## Errors and shutdown

Every function returns a `prism_rklocal_result_t`. Use
`prism_rklocal_last_error()` for the latest client-owned diagnostic string.
On shutdown, call `prism_rklocal_stop()` for active capture and then
`prism_rklocal_close()`.

The socket must exist and the application user must have read/write permission.
Version mismatch, an existing capture owner, Agent restart, socket closure, and
timeouts are normal operational errors that applications should report.
