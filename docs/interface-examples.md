# Prism Host SDK per-interface examples

[简体中文](interface-examples.zh-CN.md)

This cookbook gives every public SDK interface a concrete example. The
[quick index](development-guide.md#api-quick-index) links each interface to the
example that uses it. Related calls share one example when they must be used as
one lifecycle, such as start/read/stop or inspect/confirm/upgrade.

Unless a block says otherwise, direct `Client`, Stream, helper, and parser
examples are for Linux x86-64 and macOS arm64 and assume:

```cpp
#include <prism/usb_sdk.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace std::chrono_literals;
```

Operations that change persistent configuration, clocks, networks, or firmware
are guarded by an explicit Boolean in these examples. Obtain real user consent
before setting that Boolean. All stream and idle-only requirements still apply.

Every snippet in this cookbook is also represented in a compile-checked source
catalogue:

- [Client lifecycle and basic control](../examples/client_api_examples.cpp)
- [Configuration, acquisition, network, and update](../examples/configuration_api_examples.cpp)
- [High-level stream wrappers](../examples/stream_api_examples.cpp)
- [Helpers and all public parsers](../examples/parser_api_examples.cpp)
- [All 43 Windows Runtime API v4 entries](../examples/windows_runtime_api_examples.cpp)

GitHub Actions builds every applicable catalogue on each supported platform.

## Client lifecycle and basic control

<a id="example-client-construction"></a>
### Default and move construction

```cpp
prism::Client first;
prism::Client second = std::move(first);

prism::Client third;
third = std::move(second);
```

`Client` is movable but not copyable. A moved-from object may only be destroyed
or assigned a new value. `~Client()` closes an open device automatically when
the owning scope exits; call `close()` explicitly when close errors matter.

<a id="example-device-enumeration"></a>
### Enumerate and open a selected device

```cpp
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("no Prism device found");
}

auto client = prism::Client::open(devices.front());
// For a single-device application, the equivalent shortcut is:
// auto client = prism::Client::openFirst();
```

Use `DeviceInfo::serial_number` from enumeration to select a specific USB
device. Both static factories perform the strict SDK/Agent version handshake.

<a id="example-client-lifecycle"></a>
### Reuse one Client object

```cpp
prism::Client client;
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("no Prism device found");
}

client.openDevice(devices.front());
if (!client.isOpen()) {
  throw std::runtime_error("open did not complete");
}
const std::wstring usb_path = client.path();
const std::wstring usb_serial = client.serialNumber();
client.closeDevice();

client.openFirstDevice();
client.close();  // close() and closeDevice() are equivalent public operations.
```

<a id="example-keepalive"></a>
### Enable, query, and disable keepalive

```cpp
client.setKeepaliveEnabled(true, 1000);
if (!client.keepaliveEnabled()) {
  throw std::runtime_error("keepalive was not enabled");
}

client.setKeepaliveEnabled(false);
```

Keepalive owns the same command path as other control calls. Do not issue
uncoordinated commands from another thread.

<a id="example-device-information"></a>
### Read versions, health, clock, ping, and network information

```cpp
const prism::HelloInfo hello = client.hello();
const prism::DeviceVersions versions = client.deviceVersions();
const prism::DeviceInfo info = client.deviceInfo();
const prism::TimeInfo rk_time = client.boardTime();
const uint64_t board_sequence = client.ping();
const prism::NetworkInfo network = client.networkInfo();

std::cout << "Agent " << versions.agent
          << ", sensor-board " << versions.sensor_board
          << ", USB " << prism::usbLinkSpeedName(info.usb_speed)
          << ", ping " << board_sequence << '\n';
```

`deviceInfo()` returns the fresh device-health snapshot. Enumeration only fills
USB identity fields.

<a id="example-time-sync"></a>
### Measure time and optionally synchronize the device

```cpp
if (client.streamTransferActive()) {
  throw std::runtime_error("stop Camera, IMU, and LiDAR before time control");
}

const prism::NtpTimeSyncResult measured =
    client.synchronizeTimeNtpLike(12, 1000);
std::cout << "device-host offset: " << measured.offset_us << " us\n";

const bool user_confirmed_clock_write = false;
if (user_confirmed_clock_write) {
  const prism::SystemTimeSyncResult result =
      client.synchronizeSystemTime(12, 6, 1000);
  std::cout << "verified: " << std::boolalpha << result.verified << '\n';
}
```

The measurement call never modifies a clock. System synchronization changes
RK system/PTP/RTC time and therefore requires a correct host clock.

<a id="example-wifi"></a>
### Read and explicitly change Wi-Fi hotspot state

```cpp
if (client.streamTransferActive()) {
  throw std::runtime_error("Wi-Fi control is idle-only");
}

const prism::WifiHotspotStatus current = client.wifiHotspotStatus();
std::cout << current.ssid << " running=" << current.running << '\n';

const bool user_confirmed_wifi_change = false;
if (user_confirmed_wifi_change) {
  const prism::WifiHotspotStatus updated =
      client.setWifiHotspotEnabled(!current.enabled);
  std::cout << "enabled=" << updated.enabled << '\n';
}
```

Changing the hotspot can immediately disconnect network users. USB remains the
control transport for this operation.

## Configuration, acquisition, and update

<a id="example-device-configuration"></a>
### Read and selectively save persistent configuration

```cpp
prism::DeviceConfiguration configuration = client.deviceConfiguration();
if (!prism::isCameraFpsSupported(configuration.camera_fps)) {
  throw std::runtime_error("device returned an unsupported Camera FPS");
}

const bool user_confirmed_persistent_write = false;
if (user_confirmed_persistent_write) {
  configuration.camera_fps = 20;
  const auto saved = client.saveDeviceConfiguration(
      configuration, prism::kDeviceConfigFieldCameraFps);
  std::cout << "saved generation " << saved.generation << '\n';
}
```

Rate writes are idle-only. The field mask prevents unrelated settings from
being overwritten.

<a id="example-exposure"></a>
### Read and change runtime exposure and gain

```cpp
prism::ExposureConfiguration exposure = client.cameraExposure();
const auto persistent = client.deviceConfiguration();
const uint32_t max_exposure_us =
    prism::cameraMaxExposureUs(persistent.camera_fps);

client.setAutoExposureTargetBrightness(35);

prism::CameraExposureConfiguration camera0;
camera0.mode = prism::CameraExposureMode::Manual;
camera0.exposure_time_us = std::min<uint32_t>(2000, max_exposure_us);
camera0.gain_x1024 = prism::kCameraDefaultGainX1024;
client.setCameraExposure(0, camera0);

exposure.target_brightness = 35;
client.setExposureConfiguration(exposure, prism::kExposureFieldTargetBrightness);
```

During capture, calculate the limit from the active `VideoStatus::fps` instead
of the persistent FPS. Exposure settings are runtime-only and are not saved.

<a id="example-camera-imu-control"></a>
### Start and stop the aggregate Camera/IMU session

```cpp
const prism::DeviceInfo info = client.deviceInfo();
const prism::VideoStatus video = client.startVideo1280x1024(20);
if (!video.enabled || video.cameras != info.detected_camera_count) {
  client.stopVideo();
  throw std::runtime_error("Camera start was incomplete");
}

const prism::ImuStreamStatus imu =
    client.startImu(info.detected_imu_count, 0);
if (!imu.enabled) {
  client.stopVideo();
  throw std::runtime_error("IMU start failed");
}

const bool stop_through_imu_api = false;
if (stop_through_imu_api) {
  (void)client.stopImu();
} else {
  client.stopVideo();
}
```

Either stop call stops the shared Camera/IMU capture session.

<a id="example-video-ack"></a>
### Return Camera flow-control credit

```cpp
const uint32_t complete_frame_id = assembled_frame.frame_id;
if (assembled_frame.has_all_cameras && assembled_frame.has_metadata) {
  consume(assembled_frame);
  client.sendVideoAck(complete_frame_id);
}
```

ACK only after all JPEGs and matching metadata are received. Retire and ACK an
older incomplete frame when a newer frame ID proves the old chunks will not
arrive. See the complete
[`camera_imu_capture.cpp`](../examples/camera_imu_capture.cpp) implementation.

<a id="example-lidar-control"></a>
### Start, query, and stop LiDAR

```cpp
prism::LidarStatus started =
    client.startLidar(prism::LidarModel::Mid360);
if (!started.enabled) {
  throw std::runtime_error(started.error);
}

const prism::LidarStatus current = client.lidarStatus();
std::cout << current.point_count << " points\n";

const prism::LidarStatus stopped = client.stopLidar();
if (stopped.enabled) {
  throw std::runtime_error("LiDAR did not stop");
}
```

The model is mandatory; use `Mid360S` for that product.

<a id="example-lidar-network"></a>
### Inspect, save, and probe the LiDAR network

```cpp
if (client.streamTransferActive()) {
  throw std::runtime_error("LiDAR network control is idle-only");
}

prism::LidarNetworkStatus status = client.lidarNetworkStatus();
const bool user_confirmed_network_write = false;
if (user_confirmed_network_write) {
  prism::LidarNetworkConfiguration configuration = status.configuration;
  configuration.host_ip = "192.168.1.5";
  configuration.netmask = "255.255.255.0";
  configuration.lidar_ip = "192.168.1.3";
  status = client.saveLidarNetworkConfiguration(configuration);
}

const prism::LidarNetworkStatus probe = client.probeLidarNetwork();
std::cout << "reachable=" << probe.target_reachable << '\n';
```

`probeLidarNetwork()` tests reachability using the saved configuration; save
edited values before probing them.

<a id="example-low-level"></a>
### Read raw frames and issue a raw command

```cpp
const prism::Frame pong = client.command(prism::FrameType::Ping, {}, 3000);
std::cout << prism::frameTypeName(pong.type) << '\n';

for (;;) {
  const prism::Frame frame = client.readFrame(3000);
  std::cout << prism::frameTypeName(frame.type)
            << " sequence=" << frame.sequence << '\n';
  if (frame.type == prism::FrameType::Heartbeat) {
    break;
  }
}
```

Use one receive loop per Client. Prefer high-level methods unless implementing
a dispatcher for stream frames.

<a id="example-system-upgrade"></a>
### Inspect and explicitly perform a system upgrade

```cpp
const std::string package_path = "prism-system-update.zip";
const prism::SystemUpgradePackageInfo package =
    prism::inspectSystemUpgradePackage(package_path);
std::cout << package.package_version << '\n';

const bool user_confirmed_upgrade = false;
if (user_confirmed_upgrade) {
  if (client.streamTransferActive()) {
    throw std::runtime_error("stop all streams before upgrade");
  }
  prism::UpgradeOptions options;
  options.wait_for_restart = true;
  const auto result = client.upgradeSystem(
      package_path, options,
      [](const prism::SystemUpgradeProgress& progress) {
        std::cout << progress.completed_bytes << '/'
                  << progress.total_bytes << '\n';
      });
  if (!result.complete) {
    throw std::runtime_error("system upgrade did not complete");
  }
}
```

For low-level upgrade-status frames, use
`parseUpgradeStatus(frame)` and `parseSensorBoardUpgradeStatus(frame)` only
with their matching frame types. Keep power and USB stable throughout update.

## Stream wrapper classes

<a id="example-imu-stream"></a>
### ImuStream lifecycle

```cpp
prism::ImuStream imu(client, [](const prism::ImuSample& sample) {
  if (sample.timestamp_synced) {
    std::cout << sample.sensor_id << ' ' << sample.timestamp_us << '\n';
  }
});

const auto info = client.deviceInfo();
imu.start(info.detected_imu_count, 0);
while (imu.active()) {
  const prism::Frame frame = client.readFrame(3000);
  const bool consumed = imu.handleFrame(frame);
  if (!consumed) {
    dispatch_non_imu_frame(frame);
  }
  if (should_stop()) {
    imu.stop();
  }
}
```

Feed every received frame to `handleFrame()` from the single receive thread.
`~ImuStream()` makes a best-effort stop, but production code should call
`stop()` explicitly so an error can be reported.

<a id="example-lidar-stream"></a>
### Point-only and point-plus-IMU LidarStream lifecycles

```cpp
void capture_points_only(prism::Client& client) {
  prism::LidarStream lidar(client, [](const prism::LidarPointBatch& batch) {
    std::cout << batch.points.size() << '\n';
  });
  lidar.start(prism::LidarModel::Mid360);
  while (lidar.active() && !should_stop()) {
    (void)lidar.handleFrame(client.readFrame(3000));
  }
  lidar.stop();
}

void capture_points_and_imu(prism::Client& client) {
  prism::LidarStream lidar(
      client,
      [](const prism::LidarPointBatch& batch) { consume_points(batch); },
      [](const prism::LidarImuSample& sample) { consume_lidar_imu(sample); });
  lidar.start(prism::LidarModel::Mid360S);
  while (lidar.active() && !should_stop()) {
    const prism::Frame frame = client.readFrame(3000);
    if (!lidar.handleFrame(frame)) {
      dispatch_other_frame(frame);
    }
  }
  lidar.stop();
}
```

Run only one of these lifecycles at a time. `~LidarStream()` also attempts to
stop an active stream, but explicit `stop()` is the error-reporting path.

## Helpers and parsers

<a id="example-host-version"></a>
### Query the Host SDK version

```cpp
const std::string sdk_version = prism::hostSdkVersion();
std::cout << sdk_version << '\n';
```

<a id="example-device-name-helpers"></a>
### Convert device enums to names

```cpp
const auto info = client.deviceInfo();
std::cout << prism::usbLinkSpeedName(info.usb_speed) << '\n';
for (const auto reason : info.imu_init_error_reason) {
  std::cout << prism::imuInitErrorReasonName(reason) << '\n';
}
std::cout << prism::sensorBoardErrorCodeName(info.sensor_board_error_code)
          << '\n';
```

The returned `const char*` values are SDK-owned static strings.

<a id="example-parser-dispatch"></a>
### Dispatch every public telemetry parser

```cpp
void parse_frame(const prism::Frame& frame) {
  switch (frame.type) {
    case prism::FrameType::DeviceInfoResponse:
      consume(prism::parseDeviceInfo(frame));
      break;
    case prism::FrameType::ExposureResponse:
      consume(prism::parseExposureConfiguration(frame));
      break;
    case prism::FrameType::WifiHotspotStatus:
      consume(prism::parseWifiHotspotStatus(frame));
      break;
    case prism::FrameType::Heartbeat:
      consume(prism::parseHeartbeat(frame));
      break;
    case prism::FrameType::VideoChunk: {
      const prism::VideoChunkView view = prism::parseVideoChunkView(frame);
      consume_before_frame_destruction(view);
      const prism::VideoChunk owned = prism::parseVideoChunk(frame);
      retain_or_queue(owned);
      break;
    }
    case prism::FrameType::VideoMeta:
      consume(prism::parseVideoMeta(frame));
      break;
    case prism::FrameType::ImuSample:
      consume(prism::parseImuSample(frame));
      break;
    case prism::FrameType::LidarStatusResponse:
      consume(prism::parseLidarStatus(frame));
      break;
    case prism::FrameType::LidarNetworkStatus:
      consume(prism::parseLidarNetworkStatus(frame));
      break;
    case prism::FrameType::LidarPoints:
      consume(prism::parseLidarPointBatch(frame));
      break;
    case prism::FrameType::LidarImuSample:
      consume(prism::parseLidarImuSample(frame));
      break;
    case prism::FrameType::UpgradeStatus:
      consume(prism::parseUpgradeStatus(frame));
      break;
    case prism::FrameType::SensorBoardUpgradeStatus:
      consume(prism::parseSensorBoardUpgradeStatus(frame));
      break;
    default:
      break;
  }
}
```

Strict parsers throw if the frame type, protocol version, or payload size is
wrong. `VideoChunkView` is valid only while its source `Frame` remains alive;
`VideoChunk` owns its byte vector.

## Windows Runtime API v4

<a id="example-windows-runtime"></a>
### Load the table and call all 43 function-pointer interfaces

The complete buildable Windows loader is
[`device_info_time_sync.cpp`](../examples/device_info_time_sync.cpp). After it
loads `prism_usb_sdk.dll`, resolves `prism_usb_sdk_get_runtime_api`, validates
ABI v4/SDK 1.0.0/MSVC compatibility, and stores the result in `api`, the
function-pointer calls have these direct forms:

| Runtime API field | Corresponding example |
| --- | --- |
| `client_create` | `prism::Client* client = api->client_create();` |
| `client_destroy` | `api->client_destroy(client);` |
| `enumerate` | `auto devices = api->enumerate(prism::kDefaultVid, prism::kDefaultPid);` |
| `open_device` | `api->open_device(client, devices.front());` |
| `close_device` | `api->close_device(client);` |
| `is_open` | `bool open = api->is_open(client);` |
| `path` | `std::wstring path = api->path(client);` |
| `serial_number` | `std::wstring serial = api->serial_number(client);` |
| `keepalive_enabled` | `bool enabled = api->keepalive_enabled(client);` |
| `stream_transfer_active` | `bool active = api->stream_transfer_active(client);` |
| `hello` | `auto value = api->hello(client);` |
| `device_info` | `auto value = api->device_info(client);` |
| `device_versions` | `auto value = api->device_versions(client);` |
| `synchronize_system_time` | `auto value = api->synchronize_system_time(client, 12, 6, 1000);` |
| `network_info` | `auto value = api->network_info(client);` |
| `wifi_hotspot_status` | `auto value = api->wifi_hotspot_status(client);` |
| `set_wifi_hotspot_enabled` | `auto value = api->set_wifi_hotspot_enabled(client, true);` |
| `device_configuration` | `auto value = api->device_configuration(client);` |
| `save_device_configuration` | `auto value = api->save_device_configuration(client, configuration, field_mask);` |
| `camera_exposure` | `auto value = api->camera_exposure(client);` |
| `set_exposure_configuration` | `auto value = api->set_exposure_configuration(client, exposure, field_mask);` |
| `start_video` | `auto value = api->start_video(client, 20);` |
| `stop_video` | `api->stop_video(client);` |
| `start_imu` | `auto value = api->start_imu(client, sensor_count, 0);` |
| `stop_imu` | `auto value = api->stop_imu(client);` |
| `send_video_ack` | `api->send_video_ack(client, complete_frame_id);` |
| `start_lidar` | `auto value = api->start_lidar(client, prism::LidarModel::Mid360);` |
| `stop_lidar` | `auto value = api->stop_lidar(client);` |
| `lidar_status` | `auto value = api->lidar_status(client);` |
| `lidar_network_status` | `auto value = api->lidar_network_status(client);` |
| `save_lidar_network_configuration` | `auto value = api->save_lidar_network_configuration(client, configuration);` |
| `probe_lidar_network` | `auto value = api->probe_lidar_network(client);` |
| `read_frame` | `prism::Frame frame = api->read_frame(client, 3000);` |
| `inspect_system_upgrade_package` | `auto value = api->inspect_system_upgrade_package(package_path);` |
| `upgrade_system` | `auto value = api->upgrade_system(client, package_path, options, progress);` |
| `parse_heartbeat` | `auto value = api->parse_heartbeat(frame);` |
| `parse_video_chunk_view` | `auto value = api->parse_video_chunk_view(frame);` |
| `parse_video_meta` | `auto value = api->parse_video_meta(frame);` |
| `parse_imu_sample` | `auto value = api->parse_imu_sample(frame);` |
| `parse_lidar_point_batch` | `auto value = api->parse_lidar_point_batch(frame);` |
| `usb_link_speed_name` | `const char* name = api->usb_link_speed_name(speed);` |
| `sensor_board_error_code_name` | `const char* name = api->sensor_board_error_code_name(code);` |
| `parse_lidar_imu_sample` | `auto value = api->parse_lidar_imu_sample(frame);` |

Check every function pointer for null before use. Keep the DLL loaded until all
SDK-returned objects and the Client have been destroyed. Runtime API v4 is a
subset of the direct Linux/macOS API; interfaces absent from this table are not
available through the packaged Windows DLL.
