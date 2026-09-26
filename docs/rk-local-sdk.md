# RK-local SDK: C++ interface

[简体中文](rk-local-sdk.zh-CN.md) · [SDK README](../README.md)

The commands below use this binary distribution; no private source checkout is required.

The public RK-local interface is **C++17 `prism::rklocal::Client`**, for Linux
ARM64 applications on RK3576. It connects through `/run/prism/stream.sock`;
Agent alone owns UARTs, CSI/V4L2, time synchronization and aggregate capture.
GNSS/RTK methods share the actual Host SDK types, not parallel copies.

## Current version

Agent and both SDKs use **1.2.0**, protocol **1**. Main Runtime ABI is **18**.
Use matching current headers/libraries and rebuild consumers. Include
`<prism/rklocal_sdk.hpp>`; the public API is C++17.

## Package and build

- [Public C++ header](../include/prism/rklocal_sdk.hpp)
- [ARM64 archive](../runtime/linux-arm64/libprism_rklocal_sdk.a)
- [Imported CMake target](https://github.com/DIBULI/Prism-SDK/blob/master/cmake/PrismRkLocalSdk.cmake): `Prism::RkLocal`
- [Camera/IMU example](../examples/rklocal_capture.cpp)
- [Read-only GNSS example](../examples/rklocal_gnss_status.cpp)

From the SDK root on RK:

```sh
cmake -S rk-local-sdk -B build/rklocal -DCMAKE_BUILD_TYPE=Release
cmake --build build/rklocal --parallel
ctest --test-dir build/rklocal --output-on-failure
# Ten seconds, four cameras + IMU0, runtime 30 FPS, no image files:
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 - 1
# Save only the first four JPEGs; directory must not already exist:
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 ./first-jpegs 1
# 100 read-only snapshots, target 10 Hz:
build/rklocal/prism-rklocal-gnss-status /run/prism/stream.sock 100
```

Only use IMU count 2 if both sensors exist. Ctrl-C stops capture. A successful
GNSS query without a live fix is not a transport error. For cross-compilation add
`-DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/aarch64-linux-gnu.cmake` and optionally
`-DPRISM_AARCH64_CROSS_PREFIX=/path/to/aarch64-none-linux-gnu-`. Execute target
tests on ARM64 or an emulator, not directly as x86-64 executables.

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_rk_app LANGUAGES CXX)
include("/opt/Prism-SDK/cmake/PrismRkLocalSdk.cmake")
add_executable(my_rk_app main.cpp)
target_link_libraries(my_rk_app PRIVATE Prism::RkLocal)
```

The target supplies C++17, headers, archive, pthreads and dl. System C/C++ runtimes
are required. miniz/libcrypto are embedded; libusb/libcrypto.so/libssl.so are not required.
Source builds need target OpenSSL headers and static libcrypto.a (Ubuntu: libssl-dev).

## Host API alignment and explicit differences

These connected-client methods reuse Host control implementations, parameter/return
types and defaults. Compile tests compare same-named public interfaces and parameter/return types. This is source alignment, not ABI identity.

| Capability | Shared methods |
| --- | --- |
| Identity/versions/network | hello, deviceInfo, deviceVersions, boardTime, ping, networkInfo |
| Persistent configuration | deviceConfiguration, saveDeviceConfiguration |
| Runtime exposure | cameraExposure, setExposureConfiguration, setCameraExposure, setAutoExposureTargetBrightness, cameraExposureLimits, setCameraExposureLimits |
| GNSS/PPS/RTK | gnssTimingStatus, gnssReceptionStatus, gnssObservations, timeSyncRtkStatus, timeSyncRtkVersions, timeSyncPortStatus, setTimeSyncPortMode |
| CORS configuration / RTK control | timeSyncCorsConfiguration, saveTimeSyncCorsConfiguration, startRtk, stopRtk |
| Camera/IMU | startVideo1280x1024, startImu, stopVideo, stopImu, sendVideoAck |
| LiDAR | startLidar, stopLidar, lidarStatus, lidarNetworkStatus, saveLidarNetworkConfiguration, probeLidarNetwork |
| Raw rover stream | startRoverRtcm, stopRoverRtcm |
| Wi-Fi hotspot | wifiHotspotStatus, setWifiHotspotEnabled |
| Time/upgrade | synchronizeTimeNtpLike, synchronizeSystemTime, upgradeSystem |
| Transport/lifecycle | command, readFrame, streamTransferActive, setKeepaliveEnabled, keepaliveEnabled, path, serialNumber, isOpen, close, closeDevice |

**Not interchangeable / not guaranteed identical:**

- Host `prism::Client` uses USB discovery (enumerate/openFirst/openFirstDevice/open(DeviceInfo)).
  Local `prism::rklocal::Client` uses open(ClientOptions)/openDevice(ClientOptions).
  path() is the Unix socket path; serialNumber() is empty.
- synchronizeSystemTime() uses the calling process wall clock. Local processes share
  RK time and cannot obtain external UTC this way. Local-only
  `setDeviceTime(utc_us, timeout_ms=25000)` accepts external UTC microseconds at entry,
  advances by monotonic elapsed time, sets Sensor Board through Agent, then verifies RK/PPS/PHC.
  GPS lock or active transfer rejects writes. Opening never sets time.
  This helper reports one correction/verification; before remains default, not a multi-sample result.
- Local additionally offers startCapture/stopCapture/readFrameSet/readImu.
  Camera/IMU assembly and ACK are automatic. Default readFrame queues other raw events;
  set `raw_camera_imu_frames=true` for raw camera/IMU too, without another ACK.
  Host VideoStream/ImuStream/LidarStream/RoverRtcmStream wrappers require Host Client;
  use public parsers with local readFrame or its typed readers instead.
- Raw queues default to 256 frames / 16 MiB, dropping oldest non-critical frames on pressure.
  Inspect droppedRawFrames(); unsafe loss of upgrade progress raises an error.
  This is not a lossless recorder; duplicated raw images cost memory/CPU.
- Shared startImu() defaults to two IMUs; local-only startCapture() defaults to one.
  Either stop path stops both Camera and IMU.
- Shared command/readFrame default to 3000ms. CORS save uses 25000ms;
  RTK start/stop default to a total 20000ms budget.
  High-level calls reuse Host-specific timeouts. ClientOptions.command_timeout_ms (10000)
  only controls handshake and local aggregate-capture helpers. Timeout does not roll back
  an already-sent write: inspect state before retrying.
- Shared validation/parsing uses standard invalid_argument/logic_error/runtime_error;
  local transport/readers may throw prism::rklocal::Error, a runtime_error with ErrorCode.
  Exact exception subclasses are not guaranteed equal; catch std::exception.
  Local close/isOpen add noexcept; destruction is best effort, not proof of successful stop.
- Different class ABIs require rebuilding with matching headers/archives. Do not link
  Host and local static archives in one executable (shared codec symbols).
  Choose one backend per executable. Local packages target Linux ARM64;
  Agent enforces socket permissions and capture ownership.
- Joint ZIP upgrades require manifest.ini, prism-agent and BOOT.BIN. Local reconnects
  to the same socket; Host re-enumerates USB. A different Agent version needs matching SDK.

## Control operations

These writes require explicit application action, not automatic connection side effects.

```cpp
auto cfg = client.deviceConfiguration();
cfg.camera_fps = 30;
client.saveDeviceConfiguration(cfg, prism::kDeviceConfigFieldCameraFps); // idle, persistent
client.setAutoExposureTargetBrightness(45); // runtime-only
prism::CameraExposureConfiguration cam;
cam.mode = prism::CameraExposureMode::Manual;
cam.exposure_time_us = 1000;
cam.gain_x1024 = 2048;
client.setCameraExposure(0, cam);
client.setWifiHotspotEnabled(true);
auto network = client.lidarNetworkStatus();
network.configuration.lidar_ip = "192.168.1.194";
client.saveLidarNetworkConfiguration(network.configuration);
client.probeLidarNetwork();
client.startLidar(prism::LidarModel::Mid360S);
auto frame = client.readFrame(3000); // heartbeat/GNSS may arrive first
if (frame.type == prism::FrameType::LidarPoints) {
  auto points = prism::parseLidarPointBatch(frame);
}
client.stopLidar();
client.startRoverRtcm();
frame = client.readFrame(3000);
if (frame.type == prism::FrameType::RoverRtcm) {
  auto bytes = prism::parseRoverRtcmChunkView(frame); // view lifetime = frame lifetime
}
client.stopRoverRtcm();
// Real firmware write: idle transfer, stable power, validated package required.
client.upgradeSystem("/path/prism-system-update.zip", {},
    [](const prism::SystemUpgradeProgress& p) { /* no reentrant Client calls */ });
```

Wi-Fi matches Host hotspot query/enable/disable, not arbitrary SSID/password writes.
TimeSync supports `GnssInput`, `PpsNmeaOutput` and `Rtk`; see [mode guide](../docs/timesync-port.md).
RTK-module handles NTRIP. Both SDKs configure the saved account and start/stop positioning;
see [CORS / RTK control](../docs/rtk-module-control.md).
Rover output is CRC-validated receiver RTCM3, not echoed CORS data.

## Connection, capture and ownership

```cpp
#include <prism/rklocal_sdk.hpp>
#include <cstdio>

int main() {
  try {
    auto client = prism::rklocal::Client::open();
    const prism::GnssTimingStatus gps = client.gnssTimingStatus();
    std::printf("satellites=%u, NMEA age=%u ms\n", gps.satellites, gps.nmea_age_ms);
    prism::rklocal::CaptureConfiguration capture;
    capture.camera_fps = 30;
    capture.imu_rate_hz = 800;
    capture.imu_sensor_count = 1;
    client.startCapture(capture);
    if (auto imu = client.readImu(1000)) {
      // imu->accel_mg / gyro_mdps / timestamp_us
    }
    if (auto frames = client.readFrameSet(1000)) {
      const auto& jpeg = frames->image[0];
      (void)jpeg; // jpeg.data.get(), jpeg.size: complete JPEG
    }
    client.stopCapture();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
```

`ClientOptions` defaults: the socket above, 10000 ms command timeout, 8192 IMU
samples and 4 frame sets. Receive and 1 Hz keepalive threads are internal.
Client is move-only; opening an already-open object reports Busy. `isOpen()`
checks owned connection state, not Agent health. Close/destruction is noexcept
best-effort cleanup; explicitly call `stopCapture()` to observe stop errors.
Serialize lifecycle/control/query operations and join reader threads before
moving or closing a Client. Only one local client may own capture.

Capture defaults to Agent-configured rates (0) and one IMU. Explicit camera
rates are integer 1..30 FPS and IMU rate is 800 Hz. No persistent configuration is
changed; camera-only acquisition is not supported. FrameSet owns four complete
JPEG images plus exposure/gain/trigger metadata. It is move-only, transfers
JPEG ownership without copying image bytes, releases buffers automatically,
and may outlive Client. Move the entire FrameSet to an asynchronous consumer;
do not retain dangling image pointers. Bounded queues drop oldest local data
on overflow; monitor sample/frame IDs for continuity.

| Data | Units |
| --- | --- |
| IMU `accel_mg`, `gyro_mdps`, `temp_milli_c` | mg, millidegrees/s, milli-Celsius |
| IMU/Image/FrameSet `timestamp_us` | Sensor Board timeline, microseconds |
| Metadata `trigger_time_ns`, `exposure_us` | ns, us |
| Metadata analog/digital gain | gain multiplied by 1024 |

## GNSS and navigation

GNSS returns fix, position, satellites, DOP, NMEA age/update count, external
`time_synced`, PPS detection/validity/high width and the first valid RMC delay
after PPS. Use `message_pps_offset_us` only when `offset_fresh` (0..800000 us).
RK-PPS precision diagnostics remain private; reserved fields are not usable.
PPS valid alone is not proof of external UTC lock. Opening never sets time.

Check NMEA seen/fix/position validity and age before consuming cached coordinates.
The example treats data older than 2 seconds as stale. Coordinates are degrees
times 1e7, GGA MSL altitude/geoid separation are mm, DOP is times 1000 and UTC is
milliseconds of day. Ellipsoidal height = MSL altitude + geoid separation.

Use `gnssObservations(cursor, session)` for receiver-native GGA/ADRNAV/GST messages.
Check receiver solution type, validity and freshness. `timeSyncRtkStatus()` reports
module control state; running does not imply FIX. Use matching headers and ABI 18 libraries.

## CORS configuration and receiver control

`timeSyncCorsConfiguration()` returns the public account fields, never the password.
`saveTimeSyncCorsConfiguration(cfg)` saves a complete account on RK even if the
receiver is disconnected; check `configuration_applied` separately.
`startRtk(options)` binds explicit GGA consent to the reviewed saved generation.
`stopRtk()` stops receiver RTK/CORS while preserving timing. Neither is merely a UI toggle.

Both SDKs share types, validation, deadlines and confirmed-state semantics.
See [the complete bilingual API guide and examples](../docs/rtk-module-control.md).
Use `gnssObservations()` for receiver-native results; running does not guarantee FIX.

## Errors and timeouts

The three read methods return `std::optional<T>`: no data/timeout is
`std::nullopt`. Timeout 0 is nonblocking; `kWaitForever` waits for data or
disconnection. Raw readFrame throws on timeout. Shared controls use standard exceptions;
local transport failures throw `prism::rklocal::Error`, with diagnostic
`what()` and `code()`: InvalidArgument, System, Protocol, Timeout, Closed,
Busy, Remote or VersionMismatch. Do not interpret errors as missing GPS/frames.
