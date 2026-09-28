# LiDAR: capture, standby/wake and point data

[Documentation index](../README.md)

This guide applies to both Host and RK-local clients. All three supported models
use the same SDK lifecycle; model-specific point metadata is described below.

- [Capture versus hardware power control](#capture-vs-power)
- [Standby, wake, confirmation and timeouts](#standby-wake)
- [MID360 / MID360S line metadata](#livox-line)
- [XT32 preparation and per-point timestamps](#xt32)

<a id="capture-vs-power"></a>

## Capture is separate from hardware standby

| Intent | API | Effect |
| --- | --- | --- |
| Receive point data | `startLidar(model)` / `stopLidar()` | Starts/stops Agent data reception; does not command hardware standby |
| Inspect reception | `lidarStatus()` | Reports the Agent stream, not proof that the motor has stopped |
| Inspect hardware mode | `lidarPowerStatus(model, timeout_ms)` | Reads back the radar's working mode |
| Stop scanning / wake | `setLidarStandby(model, true/false, timeout_ms)` | Requests and confirms hardware standby/operation; does not start capture |

Stop **all** camera, board-IMU and LiDAR streams before standby or wake. After
waking, start capture explicitly when wanted. Read the
[capture example](../../examples/lidar_capture.cpp) for data reception and the
[interface cookbook](../examples/interfaces.md) for stream/parser usage.

<a id="standby-wake"></a>

## Hardware standby and wake

These operations control the radar hardware, not just the application's receive
stream. They require an Agent build containing the LiDAR power command extension;
older 1.2.0 builds reject the new commands as unsupported. A version string alone
does not establish support. Update the complete 1.2.0 package together.

Host `prism::Client` and RK-local `prism::rklocal::Client` share the C++ API:

```cpp
// Use an already opened, idle client. Choose the actual attached radar.
const auto model = prism::LidarModel::Xt32; // or Mid360 / Mid360S
auto before = client.lidarPowerStatus(model, 3000);
auto idle = client.setLidarStandby(model, true, 15000);
auto awake = client.setLidarStandby(model, false, 15000);
// Start capture separately, only when wanted.
```

- First stop all camera, board-IMU and LiDAR capture streams. These methods never
  stop or resume them automatically. Normal capture start/stop behavior is unchanged.
- Target IP and host-interface IP come from saved LiDAR network configuration;
  the interface must already be ready. These calls never change addresses, PTP,
  calibration, return mode or saved network settings.
- MID360 uses IDLE (target 2) and SAMPLING (target 1), then reads current work
  state. MID360S uses the same protocol. Product text is checked for the MID360
  family because some MID360S firmware reports `DevType:Mid-360`; capture still
  uses the actual discovery device type to distinguish MID360 and MID360S.
  HAP's SLEEP (3) is not used.
- XT32 uses the radar's Standby/Operation command and reads back its standby mode.
  This is vendor mode confirmation, not an independent motor-speed or power
  measurement. It is not a power-off command; network electronics remain powered.
- Wake does not begin Agent data capture. Rotor startup and usable point output
  can take additional time after a mode acknowledgment, particularly on XT32.
- Result `state`: Unknown=0, Running=1, Standby=2, Transitioning=3, Error=4.
  `vendor_state` retains MID360 current-work-state or XT32 standby-mode (0/1).
- Timeout is 1..30000 ms, for the whole hardware transaction. SDK transport adds
  2000 ms response margin. Repeated requests for the current confirmed state do
  not repeat the mode write. MID360 read-only UDP queries may be retried within
  that overall deadline when a reply is lost.
- On timeout or invalid/rejected readback, an exception is raised; a command may
  already have reached the radar. Query status before deciding whether to retry.
  Writes are never retried automatically. Unknown/unconfirmed is not Standby.
- Concurrent operations from other controllers should be avoided. Serialize client
  I/O; the Agent serializes power control with capture changes and upgrades.
- Host and RK-local share the C++ methods above. Windows runtime-loaded clients
  use the version-1 `prism_usb_sdk_get_lidar_power_api` extension with `query` and
  `set_standby` callbacks. The existing RuntimeApi v18 layout remains unchanged.
  Check version, table size and function pointers before use; an absent extension
  is unsupported, not a successful operation.

Protocol fixtures cover query, standby, wake, idempotency, transition, rejection,
CRC/identity checks and timeout. Hardware power consumption is not measured by
these APIs.

<a id="livox-line"></a>

## MID360 / MID360S point line

Host SDK and RK-local SDK expose the same `prism::LidarPoint` fields:

```cpp
const auto batch = prism::parseLidarPointBatch(frame);
for (const auto& point : batch.points) {
  if (point.line_valid) {
    const unsigned line = point.line; // 0, 1, 2, 3
    // Use line with this point's XYZ, intensity/tag and measurement time.
  }
}
```

The Agent assigns `line = original_udp_point_index % 4` for both Cartesian
high/low precision MID360 and MID360S packets, matching
[Livox ROS Driver 2](https://github.com/Livox-SDK/livox_ros_driver2/blob/master/src/comm/pub_handler.cpp).
This is a driver-generated scan-line identifier, not a field read from the
Livox packet, a tag-bit extraction, or the vertical ring of a spinning LiDAR.
Generation happens before queueing and batch merging. Never regenerate line
from the index within an SDK batch, a filtered cloud or a 100 ms ROS frame.

MID point payloads remain 16 bytes. Previously reserved byte 14 is `line`;
byte 15 is flags, bit 0 = valid, all other bits zero. Valid line is 0..3.
Zero/zero denotes unavailable metadata from an older producer, not a measured
line 0. A nonzero line with zero flags and unknown flags are rejected.
This extension applies to the existing MID v1/v2 point payloads and dataset
point records; it does not change their headers, timestamps or bandwidth.

`serializeLidarPoints()` preserves line and validity in recorded data. ROS
PointCloud2 publishes UINT8 `line` at offset 14 and UINT8 `line_valid` at offset
15, retaining the 20-byte point stride and UINT32 nanosecond `offset_time` at
offset 16. RK Web records the original payload, including these bytes.

XT32 remains unchanged: use `ring` 0..31 and explicit `offset_ns`; its MID
`line_valid` is false. Do not substitute MID line for XT32 ring.

Use matching Runtime ABI **18** headers and libraries for both SDK transports.
Rebuild applications; do not copy these headers over old binary SDK packages.

<a id="xt32"></a>

## XT32 setup, coordinates and point time

Host `prism::Client` and RK-local `prism::rklocal::Client` use the same
`startLidar(prism::LidarModel::Xt32)`, `stopLidar()`, `lidarStatus()` and
`prism::parseLidarPointBatch(frame)` interfaces. `Xt32 = 3` selects standard
PandarXT-32, not XT32M2X/XT16. Start/stop controls Agent reception only, not
the laser/motor. No LiDAR IMU events are produced; onboard IMU is unaffected.

### RK preparation

Use the matching current Agent 1.2.0 package. Set the RK host IP/netmask and the
actual LiDAR source IP with the LiDAR network configuration interface. The Agent
accepts unicast to RK or broadcast UDP on port 2368, filtered by source and RK
network interface. It does not change the LiDAR IP, destination, RPM or return mode.

At each new XT32 capture session the Agent reads and confirms **PTP** clock
selection through the radar's HTTP interface. If the radar is still using GPS,
the Agent selects PTP and verifies readback before accepting capture. The existing
PTP profile/domain must match RK. `PRISM_XT32_TIME_DOMAIN` must be `ptp`; the old
`utc` override is rejected. Mode selection is not proof of PTP lock.

By default the Agent retrieves this unit's calibration CSV over TCP 9347 before
capture, validates all 32 channels, and atomically stores a per-device-IP cache
under `/userdata/prism/hesai/`. A failed read does not silently use an old cache
or nominal angles. For an explicitly verified offline calibration file, set
`PRISM_XT32_CORRECTION_FILE` in the Agent service; the PTP check still runs.
Accepted CSV headings are `Laser id,Elevation,Azimuth` or
`Channel,Elevation,Azimuth`, with complete channels 1–32.

Do not interpret XT32 `time_type=3` as a Livox clock enumeration or proof of
PTP lock. The mapped sensor-domain time is usable only with `timestamp_synced`.
Physical calibration and clock configuration still require hardware validation.

### Points and time

XT32 batches have `version=3`, `time_interval_100ns=0`, 48-byte wire headers,
and 24-byte little-endian wire points. `LidarPoint` adds `ring` (0–31), signed
`offset_ns`, `return_id` (1 last, 2 strongest, 3 first), and `confidence` (the
raw vendor reserved byte, **not** a confidence percentage). XYZ uses Hesai's
+Y-forward azimuth-zero frame, +X at 90°, +Z up. Apply actual extrinsics when
combining this with other sensors. Invalid zero-distance returns remain zero.

`timestamp_raw` is the packet **tail** time in nanoseconds. Exact point time:

```cpp
#include <prism/usb/lidar_points.hpp>
auto batch = prism::parseLidarPointBatch(frame);
uint64_t raw_ns = prism::lidarPointTimestampNs(batch, i, batch.timestamp_raw);
// Check timestamp_synced and multiplication range before mapping:
uint64_t sensor_ns = prism::lidarPointTimestampNs(batch, i,
                                                batch.timestamp_utc_us * 1000ULL);
```

Negative offsets are normal. Dual returns can have equal/non-monotonic point
times within a packet. Do not interpolate the packet by point index, cast the
offset to uint32, or call each UDP packet a 10 Hz scan. `timestamp_utc_us` is
zero when XT32 mapping is unresolved, not host arrival time. Device-relative
time does not imply external UTC synchronization.

`serializeLidarPoints()` writes portable dataset bytes: 24-byte XT32 points or
unchanged 16-byte Livox points. Never dump `sizeof(LidarPoint)`; it is a C++
in-memory type, not a wire/storage ABI.

### Binary compatibility

Current Runtime API ABI is **18**. Use matching 1.2.0 headers/libraries and rebuild
applications. SDK and Agent semantic versions must match. XT32 explicit point
times and Livox line/validity metadata are included; Livox v1/v2 frames are supported.

Viewer and Web can preview and record XT32. New datasets declare
`lidar_storage=cartesian-mm-chunk-v3-with-point-time`; index columns remain the
same and model 3 unambiguously selects 24-byte points (models 1/2 use 16).
Viewer exports ROS1/ROS2 standard PointCloud2 in 100 ms time windows, retaining
both returns, `ring`, `return_id` and `vendor_reserved`; `offset_time` is uint32
nanoseconds from the message header's first point, never the signed packet-tail
offset. There is no deskew or automatic LiDAR-to-camera frame transform.
