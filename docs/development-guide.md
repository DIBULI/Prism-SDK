# Prism Agent SDK API Reference

This document describes the Prism Agent SDK `1.2.0` public C++ host API for
communicating with the Prism RK3576 USB agent.

The SDK header is:

```cpp
#include "prism/usb_sdk.hpp"
```

The SDK supports Windows WinUSB and Linux/macOS libusb-1.0. The public API is
kept independent from the host USB backend
without changing application-level code.

The current USB wire protocol is version 1. Agent and both SDKs are version `1.2.0`. Host SDK and agent semantic versions must match exactly (for
example, Host SDK `1.2.0` only communicates with agent `1.2.0`). Device opening
performs this handshake automatically and rejects the connection before
keepalive or stream traffic on any mismatch.

## IMU start/stop ownership

`ImuStream::start()` sends `MSG_CMD_IMU_START` and waits for the agent's
`IMU_STATUS(enabled=1)` response. The agent first sends `RUN,0`, applies the
requested rate, then sends `RUN,1` to sensor-board. USB IMU forwarding is enabled only
after that transaction succeeds. `ImuStream::stop()` waits for
`IMU_STATUS(enabled=0)` after the agent has sent `RUN,0` and flushed queued
samples.

`ImuSample` exposes `fsync_event`, `fsync_delay_valid`, and
`timestamp_synced`; applications do not need to decode the raw flag bits. On
an FSYNC event, `timestamp_us` is the UTC time of the tagged IMU sample.

## Requirements

- C++17 compiler
- Windows host with WinUSB access, Linux with libusb-1.0 and the supplied udev
  rule installed, or macOS with libusb-1.0
- USB device VID/PID:
  - VID: `0x2207`
  - PID: `0x1201`
- Linked libraries are handled by the SDK CMake target:
  - Windows: `setupapi`, `winusb`, `bcrypt`
  - Linux: `libusb-1.0`, OpenSSL `Crypto`, and the standard thread library
  - macOS: `libusb-1.0`; SHA-256 uses the system CommonCrypto implementation

## Quick Start

```cpp
#include "prism/usb_sdk.hpp"

#include <iostream>

int main() {
  auto client = prism::Client::openFirst();

  auto versions = client.deviceVersions();
  std::cout << versions.combined << "\n";

  auto network = client.networkInfo();
  std::cout << "board ip: " << network.ipv4 << "\n";

  return 0;
}
```

## Example naming

The table below names implementation-tree examples. This binary distribution
ships compile-checked consumer examples listed in [the example guide](examples.md),
including `prism-device-info-time-sync`, `prism-camera-imu-capture`,
`prism-gnss-rtk-status` and `prism-rtk-module-control`. No private source build is needed.

Implementation-tree equivalents:

| Example | Purpose |
| --- | --- |
| `prism-enumerate.exe` | List matching USB devices and open each one. |
| `prism-device_info.exe` | Query the runtime DeviceInfo snapshot. |
| `prism-hello.exe` | Run `HELLO` and `PING`. |
| `prism-time.exe` | Read board time. |
| `prism-network_info.exe` | Read board network information. |
| `prism-wifi_hotspot.exe` | Read, enable, or disable the persistent WiFi hotspot policy. |
| `prism-video_stats.exe` | Start MJPEG video, receive chunks, metadata, and FPS. |
| `prism-imu_stats.exe` | Start IMU streaming and report sample rates. |
| `prism-lidar_points.exe` | Receive Mid-360 or Mid-360S points; the model argument is mandatory. |
| `prism-raw_frames.exe` | Send low-level protocol commands and inspect frames. |
| `prism-system_upgrade.exe` | Inspect or install a combined agent + sensor-board system update ZIP. |

The standalone [Prism Viewer](https://github.com/DIBULI/Prism-Viewer) Qt
application links the installed Host SDK binary and uses the same `Client` API
as the examples. Its repository is the complete preview-application reference.

## Device Discovery

### `Client::enumerate`

```cpp
static std::vector<DeviceInfo> Client::enumerate(
    uint16_t vid = kDefaultVid,
    uint16_t pid = kDefaultPid);
```

Returns all matching USB devices.

`DeviceInfo::path` contains the Windows device path or a libusb bus/device
identifier on Linux and macOS. `DeviceInfo::serial_number` contains the USB
gadget serial.
`vendor_id` and `product_id` retain the IDs used during enumeration so the SDK
can reconnect to the same device after an agent restart.
Applications normally display the serial number and pass the selected
`DeviceInfo` back to `Client::open()` or `Client::openDevice()`; they do not
need to parse the path.

### `Client::openFirst`

```cpp
static Client Client::openFirst(
    uint16_t vid = kDefaultVid,
    uint16_t pid = kDefaultPid);
```

Opens the first matching device. This is the recommended path for products that
only connect to one board at a time.

### `Client::open`

```cpp
static Client Client::open(const DeviceInfo& device);
```

Opens a specific device returned by `enumerate()`.

### Explicit open/close lifecycle

Applications that keep one `Client` object can use the instance lifecycle API:

```cpp
prism::Client client;
client.openFirstDevice();

// Query status or start/stop streams while the device is open.
const auto hello = client.hello();

client.closeDevice();
```

To open a selected result from `enumerate()`, use:

```cpp
client.openDevice(device_info);
```

`openFirstDevice()` and `openDevice()` reject a second open on the same
`Client`. Call `closeDevice()` before opening another device. The original
static `openFirst()` / `open()` factories and `close()` remain supported.

All four open functions perform a fixed-size `HELLO` exchange internally.
They throw if the protocol version, application identity, header size, payload
limit, or semantic version differs. The USB handle is then released; callers
must not attempt a reduced-capability or compatibility mode.

## DeviceInfo Status Snapshot

```cpp
DeviceInfo Client::deviceInfo();
```

Returns a fresh status snapshot and is valid both while idle and while camera
or IMU streaming is active.

| Field | Meaning |
| --- | --- |
| `usb_speed`, `usb3_connected` | Negotiated USB link speed and whether it is SuperSpeed or faster. |
| `detected_imu_count`, `imu_present_mask` | IMUs detected by sensor-board. |
| `imu_receiving_mask`, `imu_time_synced_mask`, `imu_init_error_mask` | Per-IMU receive, UTC-sync and initialization state. |
| `imu_init_error_reason[0..1]` | Actionable reason for each IMU initialization failure: WHO_AM_I, configuration readback, FIFO bus, sample timeout, or unknown. |
| `detected_camera_count`, `camera_present_mask` | Cameras whose SC130GS I2C initialization completed in PL. This remains valid while capture is stopped and may report a partial population. |
| `camera_streaming_mask` | `0x0f` while the RK carrier/ISP/JPEG pipeline continues to produce complete four-camera frame sets. A previously healthy stream remains `0x0f` while four-frame host credit is exhausted, because no additional producer observation is possible under host USB back-pressure. Partial masks are never published. |
| `sensor_board_error_code`, `sensor_board_error_flags`, `sensor_board_error` | Current sensor-board/camera-pipeline failure classification, machine-readable flags, and readable detail. Covers camera/TC initialization, camera runtime, DDR FIFO/AXI, TC underflow/clock loss, missing cameras, an offline control link, and a four-camera production stall. Bit `0x01000000` is asserted after five seconds without the first produced frame set or four seconds without a subsequent set while host credit is available. Host USB ACK back-pressure is not classified as a sensor-board fault. |
| `product_serial` | Product/USB gadget serial number. |
| `imu_fps`, `camera_fps` | Current configured IMU and camera rates. |
| `sensor_board_online` | Whether the inter-board control link is current. |
| `sensor_board_time_synced` | Whether the sensor-board has a valid UTC source. |
| `sensor_board_time_sync_source` | Internal source metadata; show internal/external timing using the dedicated live timing status. See [time model](time-sync-api.md). |
| `wifi` | WiFi presence, configured enable state, AP/DHCP state, interface, SSID, address and error. |

Enumeration and device status intentionally share `DeviceInfo`: entries
returned by `enumerate()` contain transport identity only, while
`client.deviceInfo()` fills the status fields and preserves the open
device's path, USB serial, VID and PID.

## Connection Lifetime

```cpp
bool isOpen() const;
std::wstring path() const;
std::wstring serialNumber() const;
void close();
void closeDevice();
```

`Client` owns the USB handle and is move-only. It is not thread-safe; if one
application uses several threads, keep all calls on one I/O thread or protect the
client with a mutex.

`close()` and `closeDevice()` release the USB handle. Destruction also closes
the handle. Active IMU and video streams should be stopped before closing.
`serialNumber()` returns the serial belonging to the currently opened
`DeviceInfo`.

## RTK-module CORS and positioning control

Host `prism::Client` and RK-local `prism::rklocal::Client` expose the same
`timeSyncCorsConfiguration()`, `saveTimeSyncCorsConfiguration(cfg)`,
`startRtk(options)` and `stopRtk()` methods.
The Agent persists the account on RK; RTK-module performs NTRIP and feeds the receiver.
Queries never return passwords. Saving does not imply application or connection.
START requires GGA consent for the reviewed saved generation and confirms module state.
STOP preserves timing. Use `gnssObservations()` for receiver-native solutions.

See [CORS configuration, consent, start/stop and examples](rtk-module-control.md),
[TimeSync modes](timesync-port.md), and
[RTK-module versions](rtk-module-versions.md).

## Basic Commands

### `hello`

```cpp
HelloInfo hello();
```

Re-runs the same strict authenticated `HELLO` exchange and returns protocol and
firmware information.

Fields:

| Field | Description |
| --- | --- |
| `protocol_version` | USB wire protocol version. Current value is `1`. |
| `header_size` | Protocol frame header size in bytes. |
| `max_payload` | Maximum payload size accepted by the agent. |
| `app` | Agent application name. |
| `version` | Agent version string. |
| `process_id` | Current agent process ID. |
| `process_started_monotonic_us` | Process-instance marker used to verify restart. |
| `update_result` | Last agent update result: none, pending, success, rollback, or failed. |
| `update_version` | Version associated with `update_result`. |
| `sensor_board_version` | sensor-board firmware version, or `unknown` before a current control heartbeat is received. |

### `deviceVersions`

```cpp
DeviceVersions deviceVersions();
```

This is the dedicated version-information API. It is intentionally separate
from `deviceInfo()` and heartbeat, so applications do not need to infer
firmware versions from runtime status fields. It runs one `HELLO` exchange and
returns the agent and sensor-board versions together:

```cpp
const auto versions = client.deviceVersions();
std::cout << versions.agent << "\n";
std::cout << versions.sensor_board << "\n";
std::cout << versions.combined << "\n";
```

`combined` is formatted as
`agent <agent-version> / sensor-board <sensor-board-version>`. The sensor-board
reports its semantic version in the current control heartbeat. Before the
sensor-board connects, that component is reported as `unknown`.

### `boardTime`

```cpp
TimeInfo boardTime();
```

Returns board time in Unix milliseconds:

```cpp
int64_t TimeInfo::unix_ms;
```

### NTP-like host/device time measurement

```cpp
NtpTimeSyncResult synchronizeTimeNtpLike(
    uint32_t sample_count = 12,
    uint32_t timeout_ms = 1000);
SystemTimeSyncResult synchronizeSystemTime(
    uint32_t sample_count = 12,
    uint32_t verification_sample_count = 6,
    uint32_t timeout_ms = 1000);
bool streamTransferActive() const noexcept;
```

`synchronizeTimeNtpLike()` performs multiple four-timestamp exchanges and
returns the device-minus-host wall-clock offset. The SDK ranks observations by
round-trip delay, keeps the fastest third (at least three), and reports the
median offset plus median-absolute-deviation jitter. It only measures the
mapping; it does not set the host or device system clock.

`synchronizeSystemTime()` uses the same robust measurement and schedules Host
UTC in the Sensor Board fallback clock. Chrony follows the board PPS/UTC into
RK `CLOCK_REALTIME`, and `phc2sys -O 0` copies that same UTC timescale into the
Ethernet PHC. The agent verifies the system clock and PHC without writing
either clock directly. A hardware RTC is optional: when it is not populated,
`hardware_clock_set` is false and `rtc_device` is empty, but synchronization
still succeeds. The SDK then remeasures the residual offset and performs one
additional correction when the residual is larger than the USB measurement
quality permits. Success requires the system clock, PHC, and final offset
verification; it does not require an RTC.

The call is intentionally idle-only. If video, IMU, or LiDAR transfer is
active, the SDK throws `std::logic_error`, and the agent independently rejects
the request. Stop all streams before measuring:

```cpp
client.stopVideo();
client.stopImu();
client.stopLidar();

const auto sync = client.synchronizeTimeNtpLike();
// Device UTC estimate = host UTC + sync.offset_us.
std::cout << "offset=" << sync.offset_us
          << " us rtt=" << sync.round_trip_us
          << " us jitter=" << sync.jitter_us << " us\n";

const auto applied = client.synchronizeSystemTime();
std::cout << "before=" << applied.before.offset_us
          << " us residual=" << applied.after.offset_us
          << " us rtc=" << applied.rtc_device
          << " verified=" << applied.verified << "\n";
```

Both calls are idle-only. This RK clock synchronization is separate from the
sensor-board GPS/NMEA+PPS status returned by `DeviceInfo`; setting RK time
does not manufacture sensor-board or IMU timestamp-sync flags.

### `ping`

```cpp
uint64_t ping();
```

Sends a ping request and returns the board-side ping sequence or timestamp value
returned by the agent.

### `networkInfo`

```cpp
NetworkInfo networkInfo();
```

Returns board network information.

Fields:

| Field | Description |
| --- | --- |
| `flags` | Agent-defined status flags. |
| `interface_count` | Number of discovered network interfaces. |
| `hostname` | Board hostname. |
| `primary_interface` | Primary network interface name. |
| `ipv4` | Primary IPv4 address. |
| `netmask` | IPv4 netmask. |
| `gateway` | Default gateway. |
| `mac` | MAC address. |
| `dns` | DNS server list string. |
| `summary` | Human-readable network summary. |

## WiFi hotspot

The WiFi access-point API remains available in the
current protocol and is idle-only:

```cpp
prism::WifiHotspotStatus prism::Client::wifiHotspotStatus();
prism::WifiHotspotStatus prism::Client::setWifiHotspotEnabled(bool enabled);
```

Both methods require an open, version-authenticated device. They throw
`std::logic_error` before sending a command if video or IMU transfer is active,
because command responses and live streams share the USB receive endpoint.
`setWifiHotspotEnabled()` persists the requested policy and returns the
resulting status.

`WifiHotspotStatus` contains:

| Field | Description |
| --- | --- |
| `version`, `size`, `flags` | Validated status payload metadata. |
| `present` | WiFi hardware was detected; AP capability failures are reported separately. |
| `enabled` | The persisted requested hotspot policy. |
| `running` | Both the access point and DHCP service are running. |
| `ap_running` | The access point process is running. |
| `dhcp_running` | The DHCP service is running. |
| `persisted` | The requested enabled/disabled policy was saved. |
| `error_code`, `error` | Device-side activation error, if any. |
| `interface_name` | Selected WiFi interface. |
| `ssid` | Access-point SSID. |
| `address` | Access-point IPv4 address. |

Hotspot policy defaults to enabled when no saved policy exists. When detected
WiFi hardware supports AP mode, the device uses the RK CPU serial number as the
SSID and `10.42.200.1/24` as its address. The access point is an open network
with no password. DHCP is provided by `systemd-networkd`.

Missing WiFi hardware is not a hard error: the status reports `present=false`
and the agent keeps the enabled policy so it can start the hotspot if a device
appears later. An activation failure is also returned as a decoded status with
a nonzero `error_code`, preserving the other state fields for diagnostics.
This includes detected WiFi hardware that does not support AP mode.

```cpp
const auto before = client.wifiHotspotStatus();
if (!before.running) {
  const auto after = client.setWifiHotspotEnabled(true);
  if (after.error_code != 0) {
    throw std::runtime_error(after.error);
  }
}
```

### WiFi hotspot wire messages

All integer fields are little-endian. `WIFI_HOTSPOT_GET` (`0x0f`) has an empty
request payload. `WIFI_HOTSPOT_SET` (`0x10`) has this exact 8-byte payload:

| Offset | Type | Value |
| ---: | --- | --- |
| 0 | `u16` | WiFi payload version, exactly `1` |
| 2 | `u16` | payload size, exactly `8` |
| 4 | `u32` | enabled, exactly `0` or `1` |

Both requests return `WIFI_HOTSPOT_STATUS` (`0x90`) with this exact 208-byte
payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | WiFi payload version, exactly `1` |
| 2 | 2 | payload size, exactly `208` |
| 4 | 4 | flags: present, enabled, AP running, DHCP running, persisted |
| 8 | 4 | signed error code |
| 12 | 16 | NUL-terminated interface name |
| 28 | 33 | NUL-terminated SSID |
| 61 | 16 | NUL-terminated IPv4 address |
| 77 | 128 | NUL-terminated error text |
| 205 | 3 | zero reserved bytes |

The SDK strictly rejects unknown flags, unterminated strings, nonzero string
padding or reserved bytes, and payload version/size mismatches.

## Persistent device configuration

The agent owns a versioned persistent configuration at
`/var/lib/prism/device-config.conf`. It is written through a temporary file,
`fdatasync()`, atomic `rename()`, and directory `fsync()`. Configuration
survives agent and RK restarts.

Rate and MJPEG-quality changes are idle-only. Saved MJPEG quality is applied
the next time the camera pipeline starts. Exposure is runtime-only and is not
stored in the persistent device configuration.

```cpp
DeviceConfiguration deviceConfiguration();

DeviceConfiguration saveDeviceConfiguration(
    const DeviceConfiguration& configuration,
    uint32_t field_mask = kDeviceConfigFieldAll);
```

Current fields:

| Field | Supported values |
| --- | --- |
| `camera_fps` | `1`, `2`, `5`, `10`, `15`, `20`, `25`, or `30` |
| `imu_rate_hz` | `800` (fixed ICM45686 output rate) |
| `mjpeg_quality` | `1` through `99`; default `88` |
| `generation` | Agent-incremented persistent revision |
| `persisted` | `true` after a configuration file has been saved or loaded |

The field mask permits partial updates:

```cpp
auto config = client.deviceConfiguration();
config.imu_rate_hz = prism::kOnboardImuRateHz;
config = client.saveDeviceConfiguration(
    config, prism::kDeviceConfigFieldImuRateHz);
```

MJPEG quality can be updated independently while streams are stopped:

```cpp
auto config = client.deviceConfiguration();
config.mjpeg_quality = 85;
config = client.saveDeviceConfiguration(
    config, prism::kDeviceConfigFieldMjpegQuality);
```

Passing zero as a stream rate selects the persistent setting:

```cpp
client.startVideo1280x1024();  // fps=0: persistent camera_fps
client.startImu();             // rate=0: persistent imu_rate_hz
```

An explicit nonzero stream rate is a one-session override and is not written to
the configuration file. The `prism-config` example reads the current settings;
`prism-config <camera-fps> <imu-rate-hz>` saves both. Partial updates use
`prism-config --camera-fps <fps>` or
`prism-config --imu-rate 800`. MJPEG quality uses
`prism-config --mjpeg-quality <1..99>`.

### TimeSync connector modes

Query `timeSyncPortStatus()` or explicitly call `setTimeSyncPortMode(mode)`.
Modes are `GnssInput`, `PpsNmeaOutput`, and `Rtk`. Manual persisted mode is restored
on Agent restart; changing modes requires idle capture and safe wiring.
Mode application is independent of receiver readiness or CORS connection.
See [current mode documentation](timesync-port.md).

## Runtime camera exposure

Exposure control is independent from persistent configuration and is allowed
while acquisition is active. Each SC130GS gain is independently configurable
as `gain_x1024=1024..126976` (`1x..124x`) in `32` (`1/32x`) steps; the default
is `1024` (`1.0x`). Automatic mode uses the PL-managed RAW8 brightness loop to
adjust TRIG0 exposure time only and retains the configured gain.

```cpp
ExposureConfiguration cameraExposure();

ExposureConfiguration setExposureConfiguration(
    const ExposureConfiguration& configuration,
    uint32_t field_mask = kExposureFieldAll);

ExposureConfiguration setAutoExposureTargetBrightness(
    uint8_t target_brightness);

ExposureConfiguration setCameraExposure(
    uint8_t camera_index,
    const CameraExposureConfiguration& exposure);
```

The automatic target is one shared `1..255` value for all four cameras.
Manual exposure time is independently configurable for each camera from
`200` us through `floor(1000000 / camera_fps) - 5000` us. The supported
integer rates from 1 through 30 fps range from 995000 us at 1 fps to 28333 us at
30 fps. Gain is
independently configurable as `1024..126976` x1024 in steps of `32`; `1024` is
the reset default. Setters return the complete agent readback and never write
to `/var/lib/prism`.

Exposure examples:

```text
prism-config --auto-target=144
prism-config --camera 2 auto
prism-config --camera 2 manual 250
```

The current exposure wire protocol is version 2 with an exact 44-byte payload:
version/size, field mask, four-bit automatic-camera mask, shared target,
two zero reserved bytes, four little-endian manual exposure times, and four
little-endian sensor gains scaled by 1024. A legacy 28-byte v1 set request is
accepted while preserving the current gains; all responses use v2. See
[Runtime camera exposure](runtime-exposure.md).

## Automatic camera white balance

White balance is automatic and shared by all four BGGR cameras. The Agent
estimates one smoothed R/B pair and RKISP applies it in hardware. The SDK does
not expose manual white-balance mode or fixed R/B gain controls.

## Video Streaming

### Start Video

```cpp
VideoStatus startVideo1280x1024(uint32_t fps = 0);
```

Starts the product video stream. The current stream mode is four cameras,
1280x1024 per camera, MJPEG output over USB. `fps=0` uses persistent
`camera_fps`; `1`, `2`, `5`, `10`, `15`, `20`, `25`, or `30` is a
non-persistent override for this stream.
Camera and IMU are one aggregate capture session: this call enables both
sensor-board paths. Call `startImu()` immediately afterwards to confirm the
requested IMU count/rate and install the IMU handler.

Returned fields:

| Field | Description |
| --- | --- |
| `enabled` | Whether video streaming is active. |
| `cameras` | Number of video channels. |
| `fps` | Nominal stream frame rate. |
| `width` | Width per camera. |
| `height` | Height per camera. |
| `payload_size` | Agent-side payload buffer size. |

### Stop Video

```cpp
void stopVideo();
```

Stops the aggregate Camera + IMU capture session. The call waits until the
agent confirms that sensor-board acquisition is idle; a missing confirmation
raises an exception instead of reporting a false successful stop.

### Read Video Frames

After video starts, call `readFrame()` in a loop and dispatch by `FrameType`.

```cpp
auto status = client.startVideo1280x1024();

struct ImageBuffer {
  std::vector<uint8_t> jpeg;
  uint32_t received = 0;
};

std::map<std::pair<uint8_t, uint32_t>, ImageBuffer> pending;

while (running) {
  auto frame = client.readFrame(3000);

  if (frame.type == prism::FrameType::VideoChunk) {
    auto chunk = prism::parseVideoChunkView(frame);
    auto key = std::make_pair(chunk.camera_id, chunk.frame_id);
    auto& image = pending[key];

    if (image.jpeg.empty()) {
      image.jpeg.reserve(chunk.encoded_size);
    }

    image.jpeg.insert(image.jpeg.end(), chunk.data,
                      chunk.data + chunk.data_size);
    image.received += chunk.chunk_size;

    if (image.received >= chunk.encoded_size) {
      // image.jpeg now contains one complete MJPEG image for chunk.camera_id.
      client.sendVideoAck(chunk.frame_id);
      pending.erase(key);
    }
  } else if (frame.type == prism::FrameType::VideoMeta) {
    auto meta = prism::parseVideoMeta(frame);
    // Match meta.host_frame_id or meta.carrier_frame_id with received images.
  }
}

client.stopVideo();
```

### `VideoChunk`

`parseVideoChunkView()` and `parseVideoChunk()` convert a raw `Frame` into:

| Field | Description |
| --- | --- |
| `camera_id` | Camera index. |
| `format` | Encoded video format. Current stream payload is MJPEG. |
| `flags` | Agent-defined chunk flags. |
| `width` | Image width. |
| `height` | Image height. |
| `frame_id` | Frame identifier for this camera. |
| `encoded_size` | Full encoded JPEG size in bytes. |
| `chunk_offset` | Offset of this chunk inside the full JPEG. |
| `chunk_size` | Payload bytes in this chunk. |
| `timestamp_us` | Agent timestamp in microseconds. |
| `data` / `data_size` | Non-owning chunk byte range in `VideoChunkView`. |

Use `(camera_id, frame_id)` as the key when reassembling JPEG images.

High-rate receivers should use `parseVideoChunkView()`. It returns a
non-owning `VideoChunkView` whose `data` points into the source `Frame`;
consume it before that `Frame` is destroyed or modified. The compatible
`parseVideoChunk()` API returns an owning `VideoChunk` and performs a payload
copy.

### `VideoMeta`

`parseVideoMeta()` converts a metadata frame into:

| Field | Description |
| --- | --- |
| `valid` | Metadata validity flag. |
| `cameras` | Number of camera payloads represented by this metadata. |
| `host_frame_id` | Host-visible frame id. |
| `carrier_frame_id` | Raw carrier frame id from the capture path. |
| `carrier_width_bytes` | Width of the raw carrier frame in bytes. |
| `image_height_per_camera` | Raw image height per camera. |
| `meta_row_bytes` | Metadata row size in bytes. |
| `trigger_time_ns` | Shared four-camera TRIG0 rising-edge Unix UTC timestamp from the sensor-board; zero before PPS/RMC synchronization. It is not an exposure-center, MIPI-arrival, ISP, or delivery timestamp. |
| `exposure_us[4]` | Actual exposure time used by each camera for this frame, in microseconds. Valid metadata reports `50..(floor(1000000 / camera_fps) - 5000)` in both PL-auto and manual modes. |
| `analog_gain_x1024[4]` | Applied per-camera SC130GS sensor gain metadata, scaled by 1024 and configurable through the runtime exposure API. |
| `digital_gain_x1024[4]` | Read-only actual digital gain metadata, scaled by 1024. It is not configurable through the exposure API. |
| `meta_crc32` | Metadata CRC32. |

The `VIDEO_META` payload is exactly 84 bytes. Its fixed 24-byte prefix is
followed by `trigger_time_ns` at bytes 24–31, `exposure_us` at bytes 32–47,
`analog_gain_x1024` at bytes 48–63, `digital_gain_x1024` at bytes 64–79,
and `meta_crc32` at bytes 80–83.
`exposure_us` always describes the corresponding captured frame. In automatic
mode it is the exposure selected by the PL brightness loop, not zero or a
requested/default value.
`meta_crc32` is copied from the carrier metadata header: reflected IEEE CRC32
with initial/final XOR `0xffffffff`, calculated over all 88 carrier-header
bytes after the CRC field at bytes 84–87 has been set to zero.

For frame-to-metadata matching, use the frame ids supplied by the agent. Do not
match by arrival order alone, because USB chunks and metadata can be interleaved.

### Video Acknowledgement

```cpp
void sendVideoAck(uint32_t last_frame_id);
```

Notifies the agent that the host has consumed video up to `last_frame_id`.
Applications should send acknowledgements after fully reassembling and accepting
frames.

## System Heartbeat

The agent emits a minimal `Heartbeat` frame once per second. Read and parse it
with:

```cpp
auto frame = client.readFrame(3000);
if (frame.type == prism::FrameType::Heartbeat) {
  auto heartbeat = prism::parseHeartbeat(frame);
}
```

The only field is `HeartbeatStatus::rk_system_time_us`, the current RK
`CLOCK_REALTIME` value expressed as Unix UTC microseconds. Heartbeat does not
carry sensor-board, IMU, camera, WiFi, FPS, USB or serial-number status. Query
`Client::deviceInfo()` for all device status.

Run `prism-heartbeat.exe` to print one decoded heartbeat.

## IMU Streaming

The SDK starts a one-way host keepalive automatically when `Client::open()`
succeeds. It sends every second and does not add response frames to the shared
USB receive stream. If the agent receives no SDK keepalive for 5 seconds while
IMU or video is active, it stops IMU forwarding and all camera/ISP streaming.

`setKeepaliveEnabled(false)` is intended for watchdog testing. Production
applications should leave automatic keepalive enabled for the entire connection.

### Start IMU

```cpp
ImuStreamStatus startImu(uint32_t sensor_count = 2,
                         uint32_t nominal_rate_hz = 0);
```

Starts forwarding real sensor-board IMU samples. `nominal_rate_hz=0` uses the
persistent setting. The currently supported explicit and persistent IMU rates
are 500 Hz and 1000 Hz.

### Stop IMU

```cpp
ImuStreamStatus stopImu();
```

Stops IMU sample streaming and returns the final stream state. Applications
should call this before exit.

### `ImuStream` Application Interface

```cpp
prism::ImuStream imu(client, [](const prism::ImuSample& sample) {
  // Use structured sample fields directly; no protocol parsing in the app.
});
imu.start();

while (running) {
  auto frame = client.readFrame(3000);
  if (imu.handleFrame(frame)) continue;
  // Handle video, metadata and heartbeat frames here.
}

imu.stop();
```

`ImuStream` owns IMU start/stop for the application and hides IMU frame type
checks and byte parsing. `handleFrame()` returns `true` when it consumed an IMU
frame. This keeps a single USB receive loop for video and IMU while exposing
only structured `ImuSample` values to the viewer.

### `ImuSample`

| Field | Description |
| --- | --- |
| `sensor_id` | IMU sensor index. |
| `format` | Agent-defined IMU sample format. |
| `flags` | Sample flags. Bit 0 is FSYNC, bit 1 is valid FSYNC delay, bit 2 reports a preserved raw timestamp/sample gap, and bit 7 means `timestamp_us` is synchronized UTC. |
| `sample_id` | Monotonic sample counter per sensor. |
| `timestamp_us` | sensor-board timestamp in microseconds; Unix UTC when flags bit 7 is set, otherwise unsynchronized sensor-board-local time. |
| `accel_mg[3]` | Acceleration in milli-g. Axis order is board-defined. |
| `gyro_mdps[3]` | Angular velocity in milli-degree-per-second. |
| `temp_milli_c` | Temperature in milli-degree Celsius. |

Use `(sensor_id, sample_id)` to detect dropped or duplicated samples.

## System Upgrade

The SDK accepts one ZIP containing both the RK agent and sensor-board firmware.
It does not expose standalone agent or sensor-board upgrade methods.

```cpp
SystemUpgradePackageInfo inspectSystemUpgradePackage(
    const std::string& package_path);

SystemUpgradeResult upgradeSystem(
    const std::string& package_path,
    const UpgradeOptions& options = {},
    const std::function<void(const SystemUpgradeProgress&)>& progress = {});
```

`inspectSystemUpgradePackage()` performs the complete archive, manifest,
embedded agent-version, size, and SHA-256 validation without opening a USB
device.

`upgradeSystem()` requires an open, idle device whose running agent already
matches the Host SDK. It validates the package again, stages and commits the
sensor-board `BOOT.BIN`, and verifies QSPI read-back first. It then commits the
agent as the final transaction. The sensor-board image is counted twice in
progress because it crosses host-to-RK and RK-to-sensor-board links.

When `agent_version` differs from the current Host SDK version, the SDK does
not reconnect after the final agent commit: the new agent would correctly
reject the old SDK. Close the application and use the Host SDK release whose
version exactly equals the new agent. A same-version reinstall may reconnect
and verify the restarted process.

```cpp
const auto package =
    prism::inspectSystemUpgradePackage("prism-system-update.zip");

const auto result = client.upgradeSystem(
    "prism-system-update.zip",
    {},
    [](const prism::SystemUpgradeProgress& p) {
      std::cout << static_cast<unsigned>(p.phase) << " "
                << p.completed_bytes << "/" << p.total_bytes
                << " " << p.message << "\n";
    });
```

Important rules:

- The ZIP must contain exactly `manifest.ini`, `prism-agent`, and `BOOT.BIN`.
- Video and IMU streams must be stopped before the transaction.
- Both images and both manifest hashes are validated before the first write.
- The manifest agent version must match the binary's embedded
  `PRISM_AGENT_VERSION`.
- The sensor-board stage completes before the agent replacement begins.
- `SystemUpgradeResult::complete` is true after QSPI verification and a
  successful final agent commit; restart verification is additionally required
  for a same-version reinstall when requested.
- If the sensor-board stage fails, the agent is not changed. Correct the fault
  and retry the same complete package.
- The Viewer and public SDK do not accept a standalone `BOOT.BIN`.

Use the repository package-generation script to create the manifest and ZIP;
do not construct an update archive by hand.

## Low-Level Frames

Most applications should use typed helpers such as `hello()`,
`startVideo1280x1024()`, and `parseVideoChunk()`. Advanced users can use:

```cpp
Frame readFrame(uint32_t timeout_ms = 3000);
Frame command(FrameType type,
              const std::vector<uint8_t>& payload = {},
              uint32_t timeout_ms = 3000);
```

`readFrame()` reads one frame from the USB IN endpoint.

`command()` writes a command frame and waits for its matching response. It
keeps only the newest heartbeat seen while waiting and makes it available to a
subsequent `readFrame()`. It throws if the agent returns an error frame.

Frame fields:

| Field | Description |
| --- | --- |
| `type` | `FrameType` value. |
| `flags` | Agent-defined frame flags. |
| `sequence` | Protocol sequence number. |
| `payload` | Raw payload bytes. |

## Error Handling

The SDK reports errors by throwing `std::runtime_error`. Common error causes:

- Device not found or not bound to WinUSB.
- USB cable unplugged or board reset.
- Timeout while waiting for a frame.
- Protocol mismatch.
- Unexpected response type.
- Invalid or truncated payload.
- WiFi hotspot query or change attempted while streaming is active.
- Upgrade rejected because streaming is active.

Applications should stop active streams in cleanup paths:

```cpp
try {
  auto client = prism::Client::openFirst();
  client.startVideo1280x1024();
  prism::ImuStream imu(client, on_imu_sample);
  imu.start();
  // Main loop calls imu.handleFrame(frame).
  imu.stop();
  client.stopVideo();
} catch (const std::exception& e) {
  // Log e.what(), close the process-side client, and reconnect if needed.
}
```

## Recommended Integration Pattern

For a production host application:

1. Open the device with `Client::openFirst()` or a selected `DeviceInfo`; this
   already enforces exact protocol and semantic-version matching.
2. Optionally call `hello()` again to display current process/update details.
3. Call `networkInfo()` if the UI needs board network status.
4. Read or change WiFi hotspot state while the device is idle.
5. Start video and IMU only when the user enters acquisition mode.
6. Run one USB receive loop that calls `readFrame()` and dispatches by
   `FrameType`.
7. Reassemble MJPEG by `(camera_id, frame_id)`.
8. Match metadata by frame id, not by receive order.
9. Track IMU continuity by `(sensor_id, sample_id)`.
10. On exit, call `stopImu()` and `stopVideo()`.
11. Only allow `upgradeSystem()` while streams are stopped, and only accept a
    complete system update ZIP.
