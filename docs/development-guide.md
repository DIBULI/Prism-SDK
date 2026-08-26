# Prism Host SDK 1.0.0 Developer Guide

This guide is for application developers who receive only the public headers
and prebuilt shared libraries plus Linux static archives. It describes every public feature in Prism Host
SDK `1.0.0`, including lifecycle rules, data units, timestamp semantics, and
usage constraints. The device Agent must be `1.0.0` and the wire protocol must
be `1`. The SDK performs strict version validation when it opens a device and
does not provide a legacy-protocol compatibility mode.

[简体中文](development-guide.zh-CN.md)

Use the umbrella header:

```cpp
#include <prism/usb_sdk.hpp>
```

All public types are in the `prism` namespace. Smaller feature-specific headers
are available under `include/prism/usb/`.

The snippets below focus on the relevant SDK calls and may omit standard-library
includes, application callbacks, and shutdown signaling. The repository's
`examples` directory contains complete buildable programs.

<a id="api-quick-index"></a>
## Per-interface minimal examples and quick navigation

Use this directory to find the minimal call for every public entry point. The tables assume
that `client` is open and `frame` has the required type. Calls with side effects must still
follow the idle-state, explicit-confirmation, and rollback rules described later. Select
“Details” to jump directly to the relevant section.

- [Client lifecycle and base control](#api-client-control)
- [Configuration, acquisition, and update](#api-config-capture-update)
- [Stream wrappers](#api-stream-wrappers)
- [Free helpers and parsers](#api-helpers-parsers)
- [API and header index](#sdk-header-index)
- [Windows Runtime API v5](#sdk-windows-runtime)

<a id="api-client-control"></a>
### Client lifecycle and base control

| Interface | Minimal call | Details | Example |
| --- | --- | --- | --- |
| Default constructor | `prism::Client client;` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-construction) |
| `Client` destructor | `{ prism::Client client = prism::Client::openFirst(); }` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-construction) |
| Move construction/assignment | `prism::Client next = std::move(client);` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-construction) |
| `enumerate` | `auto devices = prism::Client::enumerate();` | [Device enumeration and selection](#sdk-device-open) | [Example](interface-examples.md#example-device-enumeration) |
| `openFirst` | `auto client = prism::Client::openFirst();` | [Device enumeration and selection](#sdk-device-open) | [Example](interface-examples.md#example-device-enumeration) |
| `open` | `auto client = prism::Client::open(devices.front());` | [Device enumeration and selection](#sdk-device-open) | [Example](interface-examples.md#example-device-enumeration) |
| `openFirstDevice` | `client.openFirstDevice();` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-lifecycle) |
| `openDevice` | `client.openDevice(devices.front());` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-lifecycle) |
| `closeDevice` | `client.closeDevice();` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-lifecycle) |
| `close` | `client.close();` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-lifecycle) |
| `isOpen` | `bool opened = client.isOpen();` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-lifecycle) |
| `path` | `std::wstring usb_path = client.path();` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-lifecycle) |
| `serialNumber` | `std::wstring usb_serial = client.serialNumber();` | [Client lifecycle](#sdk-client-lifecycle) | [Example](interface-examples.md#example-client-lifecycle) |
| `setKeepaliveEnabled` | `client.setKeepaliveEnabled(true, 1000);` | [Keepalive](#sdk-keepalive) | [Example](interface-examples.md#example-keepalive) |
| `keepaliveEnabled` | `bool keepalive = client.keepaliveEnabled();` | [Keepalive](#sdk-keepalive) | [Example](interface-examples.md#example-keepalive) |
| `hello` | `auto hello = client.hello();` | [HELLO and firmware versions](#sdk-hello-versions) | [Example](interface-examples.md#example-device-information) |
| `deviceInfo` | `auto info = client.deviceInfo();` | [Device health information](#sdk-device-info) | [Example](interface-examples.md#example-device-information) |
| `deviceVersions` | `auto versions = client.deviceVersions();` | [HELLO and firmware versions](#sdk-hello-versions) | [Example](interface-examples.md#example-device-information) |
| `boardTime` | `auto rk_time = client.boardTime();` | [Time, ping, and network](#sdk-time-network) | [Example](interface-examples.md#example-device-information) |
| `synchronizeTimeNtpLike` | `auto measured = client.synchronizeTimeNtpLike(12, 1000);` | [Time synchronization](#sdk-time-sync) | [Example](interface-examples.md#example-time-sync) |
| `synchronizeSystemTime` | `auto synced = client.synchronizeSystemTime(12, 6, 1000);` | [Time synchronization](#sdk-time-sync) | [Example](interface-examples.md#example-time-sync) |
| `streamTransferActive` | `bool active = client.streamTransferActive();` | [Threads and safety](#sdk-thread-safety) | [Example](interface-examples.md#example-time-sync) |
| `ping` | `uint64_t board_seq = client.ping();` | [Time, ping, and network](#sdk-time-network) | [Example](interface-examples.md#example-device-information) |
| `networkInfo` | `auto network = client.networkInfo();` | [Time, ping, and network](#sdk-time-network) | [Example](interface-examples.md#example-device-information) |
| `wifiHotspotStatus` | `auto ap = client.wifiHotspotStatus();` | [Wi-Fi hotspot management](#sdk-wifi) | [Example](interface-examples.md#example-wifi) |
| `setWifiHotspotEnabled` | `auto ap = client.setWifiHotspotEnabled(true);` | [Wi-Fi hotspot management](#sdk-wifi) | [Example](interface-examples.md#example-wifi) |

<a id="api-config-capture-update"></a>
### Configuration, acquisition, and update

| Interface | Minimal call | Details | Example |
| --- | --- | --- | --- |
| `deviceConfiguration` | `auto cfg = client.deviceConfiguration();` | [Persistent device configuration](#sdk-configuration) | [Example](interface-examples.md#example-device-configuration) |
| `saveDeviceConfiguration` | `cfg = client.saveDeviceConfiguration(cfg, prism::kDeviceConfigFieldCameraFps);` | [Persistent device configuration](#sdk-configuration) | [Example](interface-examples.md#example-device-configuration) |
| `cameraExposure` | `auto exposure = client.cameraExposure();` | [Exposure and gain](#sdk-exposure) | [Example](interface-examples.md#example-exposure) |
| `setExposureConfiguration` | `exposure = client.setExposureConfiguration(exposure, prism::kExposureFieldAll);` | [Exposure and gain](#sdk-exposure) | [Example](interface-examples.md#example-exposure) |
| `cameraExposureLimits` | `auto limits = client.cameraExposureLimits();` | [Exposure and gain](#sdk-exposure) | [Example](interface-examples.md#example-exposure) |
| `setCameraExposureLimits` | `limits = client.setCameraExposureLimits(limits, prism::kExposureLimitsFieldAll);` | [Exposure and gain](#sdk-exposure) | [Example](interface-examples.md#example-exposure) |
| `setAutoExposureTargetBrightness` | `client.setAutoExposureTargetBrightness(35);` | [Exposure and gain](#sdk-exposure) | [Example](interface-examples.md#example-exposure) |
| `setCameraExposure` | `client.setCameraExposure(0, camera);` | [Exposure and gain](#sdk-exposure) | [Example](interface-examples.md#example-exposure) |
| `startVideo1280x1024` | `auto video = client.startVideo1280x1024(0);` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-camera-imu-control) |
| `stopVideo` | `client.stopVideo();` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-camera-imu-control) |
| `startImu` | `auto imu_status = client.startImu(info.detected_imu_count, 0);` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-camera-imu-control) |
| `stopImu` | `auto imu_status = client.stopImu();` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-camera-imu-control) |
| `sendVideoAck` | `client.sendVideoAck(last_complete_frame_id);` | [JPEG assembly and ACK](#sdk-video-ack) | [Example](interface-examples.md#example-video-ack) |
| `startLidar` | `auto lidar = client.startLidar(prism::LidarModel::Mid360);` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-control) |
| `stopLidar` | `auto lidar = client.stopLidar();` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-control) |
| `lidarStatus` | `auto lidar = client.lidarStatus();` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-control) |
| `lidarNetworkStatus` | `auto net = client.lidarNetworkStatus();` | [LiDAR network](#sdk-lidar-network) | [Example](interface-examples.md#example-lidar-network) |
| `saveLidarNetworkConfiguration` | `auto net = client.saveLidarNetworkConfiguration(configuration);` | [LiDAR network](#sdk-lidar-network) | [Example](interface-examples.md#example-lidar-network) |
| `probeLidarNetwork` | `auto net = client.probeLidarNetwork();` | [LiDAR network](#sdk-lidar-network) | [Example](interface-examples.md#example-lidar-network) |
| `readFrame` | `prism::Frame frame = client.readFrame(3000);` | [Low-level Frame and commands](#sdk-low-level) | [Example](interface-examples.md#example-low-level) |
| `command` | `auto pong = client.command(prism::FrameType::Ping);` | [Low-level Frame and commands](#sdk-low-level) | [Example](interface-examples.md#example-low-level) |
| `upgradeSystem` | `auto result = client.upgradeSystem(package_path, options, progress);` | [System upgrade](#sdk-system-upgrade) | [Example](interface-examples.md#example-system-upgrade) |

<a id="api-stream-wrappers"></a>
### Stream wrappers

| Interface | Minimal call | Details | Example |
| --- | --- | --- | --- |
| `ImuStream` constructor | `prism::ImuStream imu(client, on_imu);` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-imu-stream) |
| `ImuStream` destructor | `{ prism::ImuStream imu(client, on_imu); }` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-imu-stream) |
| `ImuStream::start` | `imu.start(info.detected_imu_count, 0);` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-imu-stream) |
| `ImuStream::handleFrame` | `bool consumed = imu.handleFrame(frame);` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-imu-stream) |
| `ImuStream::active` | `bool active = imu.active();` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-imu-stream) |
| `ImuStream::stop` | `imu.stop();` | [Camera/IMU acquisition](#sdk-camera-imu) | [Example](interface-examples.md#example-imu-stream) |
| Point-only `LidarStream` constructor | `prism::LidarStream lidar(client, on_points);` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-stream) |
| Dual-callback `LidarStream` constructor | `prism::LidarStream lidar(client, on_points, on_lidar_imu);` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-stream) |
| `LidarStream` destructor | `{ prism::LidarStream lidar(client, on_points); }` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-stream) |
| `LidarStream::start` | `lidar.start(prism::LidarModel::Mid360);` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-stream) |
| `LidarStream::handleFrame` | `bool consumed = lidar.handleFrame(frame);` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-stream) |
| `LidarStream::active` | `bool active = lidar.active();` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-stream) |
| `LidarStream::stop` | `lidar.stop();` | [LiDAR streams](#sdk-lidar-stream) | [Example](interface-examples.md#example-lidar-stream) |

<a id="api-helpers-parsers"></a>
### Free helpers and parsers

| Interface | Minimal call | Details | Example |
| --- | --- | --- | --- |
| `hostSdkVersion` | `std::string version = prism::hostSdkVersion();` | [SDK version](#sdk-version) | [Example](interface-examples.md#example-host-version) |
| `frameTypeName` | `std::string name = prism::frameTypeName(frame.type);` | [Low-level Frame and commands](#sdk-low-level) | [Example](interface-examples.md#example-low-level) |
| `isCameraFpsSupported` | `bool supported = prism::isCameraFpsSupported(30);` | [Persistent device configuration](#sdk-configuration) | [Example](interface-examples.md#example-device-configuration) |
| `cameraMaxExposureUs` | `uint32_t limit = prism::cameraMaxExposureUs(30);` | [Exposure and gain](#sdk-exposure) | [Example](interface-examples.md#example-exposure) |
| `usbLinkSpeedName` | `const char* name = prism::usbLinkSpeedName(info.usb_speed);` | [Device health information](#sdk-device-info) | [Example](interface-examples.md#example-device-name-helpers) |
| `imuInitErrorReasonName` | `const char* name = prism::imuInitErrorReasonName(reason);` | [Device health information](#sdk-device-info) | [Example](interface-examples.md#example-device-name-helpers) |
| `sensorBoardErrorCodeName` | `const char* name = prism::sensorBoardErrorCodeName(code);` | [Device health information](#sdk-device-info) | [Example](interface-examples.md#example-device-name-helpers) |
| `inspectSystemUpgradePackage` | `auto package = prism::inspectSystemUpgradePackage(package_path);` | [System upgrade](#sdk-system-upgrade) | [Example](interface-examples.md#example-system-upgrade) |
| `parseDeviceInfo` | `auto value = prism::parseDeviceInfo(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseExposureConfiguration` | `auto value = prism::parseExposureConfiguration(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseExposureLimits` | `auto value = prism::parseExposureLimits(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseWifiHotspotStatus` | `auto value = prism::parseWifiHotspotStatus(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseHeartbeat` | `auto value = prism::parseHeartbeat(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseVideoChunkView` | `auto view = prism::parseVideoChunkView(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseVideoChunk` | `auto owned = prism::parseVideoChunk(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseVideoMeta` | `auto value = prism::parseVideoMeta(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseImuSample` | `auto value = prism::parseImuSample(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseLidarStatus` | `auto value = prism::parseLidarStatus(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseLidarNetworkStatus` | `auto value = prism::parseLidarNetworkStatus(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseLidarPointBatch` | `auto value = prism::parseLidarPointBatch(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseLidarImuSample` | `auto value = prism::parseLidarImuSample(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-parser-dispatch) |
| `parseUpgradeStatus` | `auto value = prism::parseUpgradeStatus(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-system-upgrade) |
| `parseSensorBoardUpgradeStatus` | `auto value = prism::parseSensorBoardUpgradeStatus(frame);` | [All parsers](#sdk-parsers) | [Example](interface-examples.md#example-system-upgrade) |

For Windows Runtime API v5, each minimal call has the form `api->field(client, ...)`.
All 45 function-pointer fields are grouped by their direct-API equivalent in
[Windows Runtime API v5](#sdk-windows-runtime).

<a id="sdk-header-index"></a>
## API index

| Header | Public functionality |
| --- | --- |
| `client.hpp` | Client lifecycle, control, acquisition, low-level Frame, upgrade |
| `common.hpp` | Constants, enums, DeviceInfo, Frame, HELLO, version helpers |
| `configuration.hpp` | Persistent configuration, field masks, FPS/quality ranges |
| `device_info.hpp` | DeviceInfo parser and status-name helpers |
| `exposure.hpp` | Runtime exposure, gain, ranges, and parser |
| `streams.hpp` | `ImuStream`, `LidarStream`, and callback types |
| `telemetry.hpp` | Camera, IMU, LiDAR, network, heartbeat data and parsers |
| `time_sync.hpp` | Time-measurement and time-setting results |
| `update.hpp` | Complete system package, progress, status, and parsers |
| `wifi.hpp` | Wi-Fi AP status and parser |
| `runtime_api.hpp` | Windows Runtime API v5 |

Complete buildable examples:

- [cross-platform device information and time synchronization](../examples/device_info_time_sync.cpp);
- [Camera/IMU capture, JPEG assembly, and ACK](../examples/camera_imu_capture.cpp);
- [LiDAR point-cloud and LiDAR-IMU statistics](../examples/lidar_capture.cpp);
- [Client lifecycle and basic control API catalogue](../examples/client_api_examples.cpp);
- [configuration, exposure, acquisition, network, and update API catalogue](../examples/configuration_api_examples.cpp);
- [high-level Stream API catalogue](../examples/stream_api_examples.cpp);
- [helper and parser API catalogue](../examples/parser_api_examples.cpp);
- [Windows Runtime API v5 catalogue](../examples/windows_runtime_api_examples.cpp).

Every public Client operation, Stream interface, parser, and Runtime API v5
function pointer is linked from the quick directory above to a corresponding
example and to its detailed chapter below. The eight source files are grouped
by feature rather than duplicating a nearly identical executable for every
convenience overload, and every source is compiled by the GitHub Actions
platform matrix.

<a id="sdk-platform-models"></a>
## 1. Platforms and API models

| Platform | Architecture | API model |
| --- | --- | --- |
| Ubuntu 22.04+ | x86-64 or arm64 | Link `libprism_usb_sdk.so` or `libprism_usb_sdk.a` and use the complete `Client` API |
| macOS 13+ | Apple Silicon arm64 | Link the SDK dylib, deploy the libusb dylib beside it, and use the complete `Client` API |
| Windows 10/11 | x64, MSVC 14.x | Load the DLL with `LoadLibraryExW` and call Runtime API v5 |

The Windows package does not include an import library, so applications cannot
link directly to `Client` member functions. See
[`examples/device_info_time_sync.cpp`](../examples/device_info_time_sync.cpp)
for a complete and safe DLL-loading flow.

Windows Runtime API v5 exposes most common control, acquisition, and parsing
features, but it does not expose:

- `boardTime()` or `ping()`;
- `synchronizeTimeNtpLike()`;
- `setKeepaliveEnabled()`;
- the `setAutoExposureTargetBrightness()` and `setCameraExposure()` convenience
  functions;
- the `ImuStream` and `LidarStream` wrappers;
- `hostSdkVersion()` (read `api->sdk_version` instead), `command()`, and
  `frameTypeName()`;
- `imuInitErrorReasonName()`, `parseDeviceInfo()`, the owning
  `parseVideoChunk()`, `parseLidarStatus()`, `parseLidarNetworkStatus()`, and
  the exposure, Wi-Fi, and upgrade-status parsers.

On Windows, use the function table's general control functions, `read_frame`,
and available parsers to implement equivalent application logic. Direct
`Client` examples in this guide target Linux and macOS unless stated otherwise.
Section 16 maps the Windows function table to the direct API.

## 2. Minimal lifecycle

<a id="sdk-version"></a>
### 2.1 Query the SDK version

```cpp
std::cout << prism::hostSdkVersion() << '\n';  // 1.0.0
```

<a id="sdk-device-open"></a>
### 2.2 Enumerate and select a device by serial number

```cpp
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("no Prism device found");
}

const std::wstring wanted = L"1b78953e9fc85455";
const auto it = std::find_if(
    devices.begin(), devices.end(), [&](const prism::DeviceInfo& device) {
      return device.serial_number == wanted;
    });
if (it == devices.end()) {
  throw std::runtime_error("requested serial number is not connected");
}
auto client = prism::Client::open(*it);
```

Pass custom VID and PID values to `enumerate(vid, pid)` to override the default
`0x2207:0x1201` identity. Enumeration fills only the USB identity fields:
`path`, `serial_number`, `vendor_id`, and `product_id`. A path can change after
replugging the device, so applications should use `serial_number` as the stable
selection key.

A single-device application can use:

```cpp
auto client = prism::Client::openFirst();
```

<a id="sdk-client-lifecycle"></a>
### 2.3 Reuse one Client object

```cpp
prism::Client client;
client.openFirstDevice();
std::wcout << client.path() << L' ' << client.serialNumber() << L'\n';
if (!client.isOpen()) throw std::runtime_error("device is not open");
client.closeDevice();
```

`openDevice(device)` and `close()` are also available; `close()` and
`closeDevice()` are equivalent. Opening an already-open client throws
`std::logic_error`. `Client` is movable but not copyable. Every stream wrapper
must be destroyed before the `Client` it references.

## 3. Versions, health, and basic information

<a id="sdk-hello-versions"></a>
### 3.1 HELLO and firmware versions

```cpp
const prism::HelloInfo hello = client.hello();
const prism::DeviceVersions versions = client.deviceVersions();

std::cout << hello.app << ' ' << hello.version << '\n'
          << "protocol=" << hello.protocol_version << '\n'
          << "agent=" << versions.agent << '\n'
          << "sensor-board=" << versions.sensor_board << '\n'
          << versions.combined << '\n';
```

`hello()` also returns the maximum payload, Agent PID, process-start monotonic
time, and the previous upgrade result. Use
`process_id + process_started_monotonic_us` to detect an Agent restart.

<a id="sdk-device-info"></a>
### 3.2 Fresh DeviceInfo status snapshot

```cpp
const prism::DeviceInfo info = client.deviceInfo();

std::cout << "USB=" << prism::usbLinkSpeedName(info.usb_speed) << '\n'
          << "USB3=" << info.usb3_connected << '\n'
          << "sensor-board=" << info.sensor_board_online << '\n'
          << "time-synced=" << info.sensor_board_time_synced << '\n'
          << "cameras=" << unsigned(info.detected_camera_count) << '\n'
          << "imus=" << unsigned(info.detected_imu_count) << '\n'
          << "error="
          << prism::sensorBoardErrorCodeName(info.sensor_board_error_code)
          << '\n';

for (size_t index = 0; index < info.imu_init_error_reason.size(); ++index) {
  std::cout << "imu" << index << '='
            << prism::imuInitErrorReasonName(info.imu_init_error_reason[index])
            << '\n';
}
```

`deviceInfo()` may fetch a fresh health snapshot while Camera/IMU streaming is
active. The dedicated `wifiHotspotStatus()` call is unavailable then, but
`info.wifi` can still be displayed. Serialize the query on the same I/O-owner
thread so it cannot race the single USB receive loop.

Before acquisition, check:

- `usb3_connected` for Camera use; USB 2 is generally insufficient for four
  high-rate cameras;
- `sensor_board_online`;
- `sensor_board_time_synced` when synchronized sensor time is required;
- `camera_present_mask`, `imu_present_mask`, and the detected counts;
- `imu_init_error_mask == 0`;
- `sensor_board_error_flags == 0`.

Do not hard-code two onboard IMUs. Derive the requested count from
`detected_imu_count`; the valid request range is 1..2.

`usbLinkSpeedName()`, `imuInitErrorReasonName()`, and
`sensorBoardErrorCodeName()` are display helpers. Use enum values for business
logic.

<a id="sdk-time-network"></a>
### 3.3 Time, ping, and network information

```cpp
const prism::TimeInfo time = client.boardTime();
const uint64_t pong_sequence = client.ping();
const prism::NetworkInfo network = client.networkInfo();

std::cout << "rk_unix_ms=" << time.unix_ms << '\n'
          << "pong=" << pong_sequence << '\n'
          << "host=" << network.hostname << '\n'
          << "interface=" << network.primary_interface << '\n'
          << "ipv4=" << network.ipv4 << '\n'
          << "gateway=" << network.gateway << '\n';
```

<a id="sdk-keepalive"></a>
### 3.4 Keepalive

After a device is opened, the SDK sends a keepalive every 1000 ms by default.
Production applications should leave it enabled.

```cpp
std::cout << client.keepaliveEnabled() << '\n';
client.setKeepaliveEnabled(true, 1000);  // valid interval: 100..4000 ms
```

`setKeepaliveEnabled(false)` is primarily for watchdog testing. If keepalives
are absent for too long, the Agent stops acquisition, so disabling them is not
a normal power-saving mechanism.

<a id="sdk-configuration"></a>
## 4. Persistent device configuration

```cpp
prism::DeviceConfiguration config = client.deviceConfiguration();
std::cout << config.camera_fps << ' '
          << config.imu_rate_hz << ' '
          << config.mjpeg_quality << '\n';
```

Supported ranges:

- Camera FPS: any integer from `1` through `30`; call
  `isCameraFpsSupported(fps)` when validating user input;
- IMU rate: `800` Hz (the fixed ICM45686 output rate);
- MJPEG quality: `1..99`, default `88`.

Save selected fields and read back the applied configuration:

```cpp
if (!prism::isCameraFpsSupported(20)) {
  throw std::invalid_argument("fps");
}

config.camera_fps = 20;
config = client.saveDeviceConfiguration(
    config, prism::kDeviceConfigFieldCameraFps);

config.mjpeg_quality = 89;
config = client.saveDeviceConfiguration(
    config, prism::kDeviceConfigFieldMjpegQuality);

if (!config.persisted) {
  throw std::runtime_error("configuration was not persisted");
}
```

Use `kDeviceConfigFieldAll` to update all three fields together. `generation`
is a device-maintained revision number. Persistent configuration writes require
Camera, onboard IMU, and LiDAR to be stopped. A new MJPEG quality takes effect
when the Camera pipeline next starts.

A zero in `startVideo1280x1024(0)` or `startImu(count, 0)` means "use the saved
value." A nonzero value overrides the saved value only for that acquisition
session.

<a id="sdk-exposure"></a>
## 5. Runtime exposure and gain

Exposure configuration is not persistent and returns to its default after an
Agent or sensor-board restart. It can be read or changed during acquisition.

### 5.1 Read all four camera states

```cpp
const prism::ExposureConfiguration exposure = client.cameraExposure();
for (size_t camera = 0; camera < 4; ++camera) {
  const bool automatic =
      (exposure.automatic_camera_mask & (1u << camera)) != 0;
  std::cout << "cam" << camera
            << " auto=" << automatic
            << " exposure_us=" << exposure.manual_exposure_time_us[camera]
            << " gain=" << exposure.gain_x1024[camera] / 1024.0
            << "x\n";
}
```

### 5.2 Change the automatic-exposure target brightness

```cpp
client.setAutoExposureTargetBrightness(35);  // valid range: 1..255
```

The target brightness is shared by all four cameras.

### 5.3 Set one camera to manual exposure and gain

```cpp
const auto status = client.deviceInfo();
const uint32_t fps = status.camera_fps != 0
                         ? status.camera_fps
                         : client.deviceConfiguration().camera_fps;
const uint32_t max_us = prism::cameraMaxExposureUs(fps);

prism::CameraExposureConfiguration camera;
camera.mode = prism::CameraExposureMode::Manual;
camera.exposure_time_us = std::min<uint32_t>(2000, max_us);
camera.gain_x1024 = 2048;  // 2.0x
client.setCameraExposure(0, camera);
```

The hardware ranges are 50..995000 us and gain 1024..126976 (1x..124x) in
steps of 32. The current configured limits can further narrow those ranges,
and the effective maximum exposure never exceeds
`floor(1000000 / fps) - 5000` us.

Return one camera to automatic mode:

```cpp
camera.mode = prism::CameraExposureMode::Automatic;
client.setCameraExposure(0, camera);
```

### 5.4 Write all fields in one request

```cpp
auto all = client.cameraExposure();
all.target_brightness = 40;
all.automatic_camera_mask = 0x0f;
all.gain_x1024.fill(1024);
all = client.setExposureConfiguration(all, prism::kExposureFieldAll);
```

Field masks include `kExposureFieldTargetBrightness`, individual Camera 0..3
bits, `kExposureFieldCameraAll`, and `kExposureFieldAll`.

### 5.5 Configure automatic exposure and gain limits

```cpp
prism::ExposureLimits limits = client.cameraExposureLimits();
limits.min_exposure_time_us = 500;
limits.max_exposure_time_us = 20000;
limits.min_gain_x1024 = 1024;   // 1x
limits.max_gain_x1024 = 8192;   // 8x
limits = client.setCameraExposureLimits(
    limits, prism::kExposureLimitsFieldAll);
std::cout << "effective max exposure="
          << limits.effective_max_exposure_time_us << " us\n";
```

These limits are shared by all four cameras and are runtime-only. Automatic
control raises exposure first, then gain after reaching the effective exposure
maximum. When reducing brightness it lowers gain first, then exposure. The
Agent clamps existing manual values into the new range. An FPS change is
rejected when its frame-period limit is below the configured minimum exposure.

<a id="sdk-time-sync"></a>
## 6. Time measurement and system-time synchronization

Both operations require every data stream to be stopped.

### 6.1 Measure without modifying a clock

```cpp
if (client.streamTransferActive()) {
  throw std::logic_error("stop every data stream first");
}
const prism::NtpTimeSyncResult measured =
    client.synchronizeTimeNtpLike(12, 1000);
std::cout << "device_minus_host_us=" << measured.offset_us << '\n'
          << "round_trip_us=" << measured.round_trip_us << '\n'
          << "jitter_us=" << measured.jitter_us << '\n';
```

`TimeSyncSample` stores the four timestamps, round-trip time, and offset for
each exchange.

### 6.2 Set device time from the host

```cpp
if (client.streamTransferActive()) {
  throw std::logic_error("stop every data stream first");
}
// Run only after an explicit user request and after validating host time.
const prism::SystemTimeSyncResult result =
    client.synchronizeSystemTime(12, 6, 1000);
std::cout << "before=" << result.before.offset_us << '\n'
          << "applied=" << result.applied_correction_us << '\n'
          << "after=" << result.after.offset_us << '\n'
          << "verified=" << result.verified << '\n';
```

This operation changes RK `CLOCK_REALTIME`, the Ethernet PTP hardware clock,
and the RK RTC. It does not change the host clock and does not replace
sensor-board GPS/NMEA+PPS synchronization. Do not call it when the host time is
not trustworthy. A verification failure throws; after a normal return,
`verified` should be true.

Both sample counts accept 3..64. `timeout_ms` accepts 100..10000.

<a id="sdk-wifi"></a>
## 7. Wi-Fi hotspot management

The Wi-Fi API manages only the device access point. It does not provide Wi-Fi
sensor-data transport. Reading or changing hotspot state requires all data
streams to be stopped.

```cpp
const prism::WifiHotspotStatus status = client.wifiHotspotStatus();
std::cout << status.present << ' '
          << status.enabled << ' '
          << status.running << ' '
          << status.ssid << ' '
          << status.address << '\n';
```

Explicitly change the state and check device-side errors:

```cpp
const auto changed = client.setWifiHotspotEnabled(true);
if (changed.error_code != 0) {
  throw std::runtime_error(changed.error);
}
```

`enabled` is the persisted policy. `running` is true only when both the access
point and DHCP are running. `present=false` is a status that applications can
display; it is not a protocol error.

<a id="sdk-camera-imu"></a>
## 8. Aggregate Camera and onboard-IMU acquisition

Camera and onboard IMU belong to one aggregate acquisition session.
`startVideo1280x1024()` and `startImu()` both make the two paths active. Either
`stopVideo()` or `stopImu()` stops the complete aggregate session.

The recommended order is to start Camera and then confirm IMU selection using
the actual detected count:

```cpp
const auto info = client.deviceInfo();
if (info.detected_imu_count == 0) {
  throw std::runtime_error("no onboard IMU is available");
}

const prism::VideoStatus video = client.startVideo1280x1024(0);
if (!video.enabled || video.cameras == 0 || video.width != 1280 ||
    video.height != 1024) {
  throw std::runtime_error("Camera did not start as requested");
}
prism::ImuStream imu(client, [](const prism::ImuSample& sample) {
  // The callback runs synchronously in the handleFrame() caller.
  consumeImu(sample);
});
imu.start(info.detected_imu_count, 0);
```

`VideoStatus` returns the actual enabled state, camera count, FPS, dimensions,
and payload size. Image dimensions are fixed at 1280x1024. `fps=0` uses the
persistent configuration; an explicit FPS can be any integer from 1 through 30.

### 8.1 The single receive loop

All data shares one USB IN endpoint, so there must be exactly one
`readFrame()` reader:

```cpp
while (running) {
  prism::Frame frame = client.readFrame(3000);

  if (imu.handleFrame(frame)) continue;

  switch (frame.type) {
    case prism::FrameType::VideoChunk:
      acceptChunk(prism::parseVideoChunkView(frame));
      break;
    case prism::FrameType::VideoMeta:
      acceptMeta(prism::parseVideoMeta(frame));
      break;
    case prism::FrameType::Heartbeat:
      consumeHeartbeat(prism::parseHeartbeat(frame));
      break;
    default:
      break;
  }
}
```

`ImuStream::handleFrame()` returns true for IMU frames and false for other
frames. Use `active()` to query state. Repeated `start()` and `stop()` calls are
safe. The destructor attempts to stop the stream but suppresses stop errors, so
production code should call `stop()` explicitly.

For a complete, buildable Camera/IMU receive-loop, chunk-assembly, and ACK
example, see
[`examples/camera_imu_capture.cpp`](../examples/camera_imu_capture.cpp).

<a id="sdk-video-ack"></a>
### 8.2 JPEG chunk assembly and ACK

`VideoChunkView` is a zero-copy view. Its `data` remains valid only while the
source `Frame` has not been destroyed, moved, or modified. Copy the bytes before
asynchronous decoding or crossing a thread boundary. Use `parseVideoChunk()`
when an owning result is more convenient.

Assembly rules:

1. Key state by `(camera_id, frame_id)`.
2. Allocate `encoded_size` bytes.
3. Write `data_size` bytes at `chunk_offset`.
4. Validate bounds, duplicates, and overlaps.
5. After all four JPEGs and metadata with the same `host_frame_id` have arrived,
   ACK continuous frame IDs in order.
6. Send the ACK before JPEG decoding, rendering, or disk writing.

```cpp
if (frameSetComplete(next_frame_id)) {
  client.sendVideoAck(next_frame_id);
  ++next_frame_id;
}
```

Once the ordered USB stream has moved to a newer `frame_id`, chunks for an older
partial frame will not return. Explicitly retire that older frame as dropped
and ACK its own credit, while keeping ACK IDs continuous. Do not implicitly
skip it by ACKing only a newer ID, and do not wait forever for it. Metadata
arrival satisfies credit completeness even when `meta.valid=false`; such a
frame must still be ACKed but must not be consumed as valid sensor data. Clear
all assembly state at the start of each capture.

<a id="sdk-camera-metadata"></a>
### 8.3 Camera metadata and physical time

```cpp
const prism::VideoMeta meta = prism::parseVideoMeta(frame);
const bool camera_time_synced =
    client.deviceInfo().sensor_board_time_synced;
if (meta.valid && camera_time_synced && meta.trigger_time_ns != 0) {
  const uint64_t camera_time_ns = meta.trigger_time_ns;
}
```

`trigger_time_ns` is the common TRIG0 edge for all four cameras. It is not the
exposure midpoint, ISP-completion time, or USB-arrival time.
`VideoMeta::valid` validates the metadata structure; it does not prove that the
clock is synchronized. Camera time is alignable only when
`DeviceInfo::sensor_board_time_synced` is also true. Use
`host_frame_id` to match JPEG data. `exposure_us`, `analog_gain_x1024`, and
`digital_gain_x1024` contain the actual values for that frame.

<a id="sdk-onboard-imu"></a>
### 8.4 Onboard IMU data

`ImuSample` uses these units:

- `accel_mg`: milli-g;
- `gyro_mdps`: milli-degrees per second;
- `temp_milli_c`: milli-degrees Celsius;
- `timestamp_us`: microseconds.

```cpp
const double ax_m_s2 = sample.accel_mg[0] * 9.80665 / 1000.0;
const double gx_rad_s =
    sample.gyro_mdps[0] * 3.14159265358979323846 / 180000.0;
```

Compare this time with synchronized Camera or LiDAR time only when
`timestamp_synced=true`, equivalently when `flags & kImuFlagTimestampSynced` is
nonzero. Use `(sensor_id, sample_id)` to check continuity. `fsync_event` marks
the first ODR sample after an FSYNC edge, `fsync_delay_valid` indicates a valid
delay field, and `sample_gap` reports a detected gap longer than four ODR
periods.

Stop the aggregate session with one stop call:

```cpp
imu.stop();  // Stops Camera and onboard IMU together.
```

<a id="sdk-lidar-network"></a>
## 9. LiDAR network

Reading, saving, and probing LiDAR network state are idle-only operations.

```cpp
const auto status = client.lidarNetworkStatus();
std::cout << status.interface_name << ' '
          << status.configuration.host_ip << ' '
          << status.configuration.lidar_ip << '\n';
```

Save the `end0` configuration:

```cpp
if (client.streamTransferActive()) {
  throw std::logic_error("stop every data stream first");
}
const auto previous = client.lidarNetworkStatus();  // Keep for rollback.
std::cout << "previous host IP=" << previous.configuration.host_ip << '\n';
// Continue only after explicit user confirmation of the new addresses.
prism::LidarNetworkConfiguration network;
network.enabled = true;
network.host_ip = "192.168.1.5";
network.netmask = "255.255.255.0";
network.lidar_ip = "192.168.1.3";
const auto saved = client.saveLidarNetworkConfiguration(network);
if (!saved.persisted) {
  throw std::runtime_error("LiDAR network configuration was not saved");
}
```

Saving changes the persistent RK `end0` configuration. Show the previous value
to the user and provide rollback; do not unconditionally write hard-coded
defaults at application startup.

Run the device-side connection test:

```cpp
const auto probe = client.probeLidarNetwork();
if (!probe.target_reachable) {
  std::cerr << probe.error << '\n';
}
```

`target_reachable=false` together with `error_code=0` means that this connection
test has not been completed, not that it failed. Display `link_up`,
`address_applied`, `same_subnet`, and `target_reachable` separately.

<a id="sdk-lidar-stream"></a>
## 10. LiDAR point cloud and LiDAR IMU

The application must select the model explicitly. The Agent does not guess it
from discovery:

```cpp
prism::LidarStream lidar(
    client,
    [](const prism::LidarPointBatch& batch) {
      if (!batch.timestamp_synced) return;
      std::cout << batch.batch_id << ' ' << batch.points.size() << '\n';
    },
    [](const prism::LidarImuSample& sample) {
      if (!sample.timestamp_synced) return;
      std::cout << sample.gyro_rad_s[0] << '\n';
    });

lidar.start(prism::LidarModel::Mid360);  // Or Mid360S.
while (running) {
  const prism::Frame frame = client.readFrame(3000);
  if (lidar.handleFrame(frame)) continue;
  // Continue dispatching Camera, onboard IMU, and heartbeat frames.
}
const prism::LidarStatus status = client.lidarStatus();
lidar.stop();
```

`LidarStream` requires at least one callback. Callbacks run synchronously in the
`handleFrame()` caller. Do not render a point cloud, perform long disk writes,
or do other blocking work in a callback.
When only points are needed, use the point-only
`LidarStream(client, point_handler)` overload. `lidar.active()` reports the
wrapper's local state. See
[`examples/lidar_capture.cpp`](../examples/lidar_capture.cpp) for a complete,
buildable point-cloud and LiDAR-IMU statistics example.

`LidarPoint` coordinates are in millimeters and include reflectivity and the raw
tag. Batch point counts vary and must not be hard-coded. `timestamp_utc_us` is
the batch base time and is reliable only when `timestamp_synced=true`.
`time_interval_100ns` is the total first-to-last point span in a batch, not the
spacing between individual points. The SDK does not expand per-point timestamps
or deskew the batch.

`timestamp_raw`/`timestamp_raw_ns`, `time_type`, and `tai_offset_applied`
preserve the original LiDAR time representation and normalization record.
Normal fusion should use a synchronized `timestamp_utc_us`. Do not compare raw
values directly with Camera/onboard-IMU time unless the Livox `time_type` and
the active clock source are explicitly understood.

`LidarImuSample` already uses SI units: gyroscope values are rad/s and
accelerometer values are m/s². It is not the same type and does not use the same
units as onboard `ImuSample`.

`LidarStatus::receiving` alone is not enough to establish point-cloud health.
Also verify that `point_count` continues to increase, batches are nonempty, and
monitor `dropped_point_count`. The low-level control API can be used directly:

```cpp
client.startLidar(prism::LidarModel::Mid360);
const auto running_status = client.lidarStatus();
const auto stopped_status = client.stopLidar();
```

<a id="sdk-combined-streams"></a>
## 11. Combined Camera, IMU, and LiDAR acquisition

All three sources share one `readFrame()` loop:

```cpp
auto client = prism::Client::openFirst();
const auto info = client.deviceInfo();
if (info.detected_imu_count == 0 || info.detected_imu_count > 2) {
  throw std::runtime_error("invalid onboard-IMU count");
}

prism::ImuStream imu(client, onBoardImu);
prism::LidarStream lidar(client, onPoints, onLidarImu);

const auto video_status = client.startVideo1280x1024(0);
if (!video_status.enabled || video_status.cameras == 0) {
  throw std::runtime_error("Camera did not start");
}
imu.start(info.detected_imu_count, 0);
lidar.start(prism::LidarModel::Mid360);
if (!client.lidarStatus().enabled) {
  throw std::runtime_error("LiDAR did not start");
}

while (running) {
  auto frame = client.readFrame(3000);
  if (imu.handleFrame(frame)) continue;
  if (lidar.handleFrame(frame)) continue;
  dispatchCameraOrHeartbeat(frame);
}

lidar.stop();
imu.stop();
client.close();
```

The cleanup order is LiDAR, the aggregate Camera/IMU session, stream objects,
then `Client`. Apply the same cleanup on exception paths. When callback data
must cross threads, copy the required data into a bounded queue. Define an
explicit drop policy for a full queue instead of blocking the USB reader.

Convert synchronized fields to a common nanosecond unit:

```cpp
const uint64_t camera_ns = meta.trigger_time_ns;
const uint64_t board_imu_ns = sample.timestamp_us * 1000u;
const uint64_t lidar_ns = batch.timestamp_utc_us * 1000u;
const uint64_t lidar_imu_ns = lidar_imu.timestamp_utc_us * 1000u;
```

Camera requires all of `meta.valid`, `meta.trigger_time_ns != 0`, and
`info.sensor_board_time_synced`. Check the separate `timestamp_synced` field on
onboard IMU, LiDAR points, and LiDAR IMU. Do not substitute
`VideoChunk::timestamp_us` for Camera trigger time, and do not mix
unsynchronized or raw LiDAR time into the same dataset.

<a id="sdk-low-level"></a>
## 12. Heartbeat, Frame, and low-level commands

### 12.1 Heartbeat

```cpp
const auto frame = client.readFrame(3000);
if (frame.type == prism::FrameType::Heartbeat) {
  const auto heartbeat = prism::parseHeartbeat(frame);
  std::cout << heartbeat.rk_system_time_us << '\n';
}
```

Heartbeat contains only the RK wall clock. It does not contain complete device
health; query `deviceInfo()` for health status.

### 12.2 Frame logging

```cpp
std::cout << prism::frameTypeName(frame.type)
          << " seq=" << frame.sequence
          << " payload=" << frame.payload.size() << '\n';
```

### 12.3 Raw command

```cpp
const prism::Frame pong = client.command(prism::FrameType::Ping);
```

Normal applications should prefer typed `Client` methods. `command()` is
intended for diagnostics; do not construct undocumented payloads. It waits for
a matching response within the requested timeout and reports protocol or Agent
errors as exceptions.

<a id="sdk-parsers"></a>
## 13. All public parsers

Every parser is strict. It throws if the `FrameType`, payload length, version,
or a field is invalid. Dispatch by type before calling a parser:

```cpp
switch (frame.type) {
  case prism::FrameType::Heartbeat:
    consume(prism::parseHeartbeat(frame));
    break;
  case prism::FrameType::DeviceInfoResponse:
    consume(prism::parseDeviceInfo(frame));
    break;
  case prism::FrameType::VideoChunk:
    consume(prism::parseVideoChunkView(frame));
    // For an owning payload: prism::parseVideoChunk(frame)
    break;
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
  case prism::FrameType::ExposureResponse:
    consume(prism::parseExposureConfiguration(frame));
    break;
  case prism::FrameType::WifiHotspotStatus:
    consume(prism::parseWifiHotspotStatus(frame));
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
```

Applications normally do not call status and configuration parsers directly,
because the corresponding `Client` method already sends the command and parses
the response.

<a id="sdk-system-upgrade"></a>
## 14. System upgrade

The public SDK accepts only a complete ZIP containing both Agent and
sensor-board firmware. It does not expose public APIs for upgrading an
individual component.

### 14.1 Inspect a package offline

```cpp
const auto package =
    prism::inspectSystemUpgradePackage("prism-system-update.zip");
std::cout << package.package_version << ' '
          << package.agent_version << ' '
          << package.sensor_board_version << '\n';
```

Package inspection does not require an open device.

### 14.2 Perform an upgrade

```cpp
const std::string package_path = "prism-system-update.zip";
const auto package = prism::inspectSystemUpgradePackage(package_path);
std::cout << package.agent_version << ' '
          << package.sensor_board_version << '\n';
if (client.streamTransferActive()) {
  throw std::logic_error("stop every data stream first");
}
// Continue only after the user confirms both package versions and power is stable.
prism::UpgradeOptions options;
options.version.clear();  // Reserved by public upgradeSystem().
options.chunk_size = 256 * 1024;
options.wait_for_restart = true;
options.restart_timeout_ms = 45000;

const auto result = client.upgradeSystem(
    package_path, options,
    [](const prism::SystemUpgradeProgress& progress) {
      std::cout << static_cast<unsigned>(progress.phase) << ' '
                << progress.completed_bytes << '/'
                << progress.total_bytes << ' '
                << progress.message << '\n';
    });
if (!result.complete) {
  throw std::runtime_error("system upgrade did not complete");
}
```

All streams must be stopped before an upgrade. The execution order is
`ValidatingPackage → SensorBoard → Agent → Complete`; enum numeric order
does not define transfer order. After an upgrade, the Agent version must still
match the Host SDK exactly; an older application will otherwise reject the
connection by design.

`chunk_size` accepts 1..1048576 bytes. With `wait_for_restart=true`,
`restart_timeout_ms` accepts 5000..300000 ms. The progress callback runs
synchronously on the calling thread; keep it fast, do not throw, and do not
reenter the same `Client`.

`parseUpgradeStatus()` and `parseSensorBoardUpgradeStatus()` are for custom
low-level upgrade tools. Normal applications should use `upgradeSystem()`.

<a id="sdk-thread-safety"></a>
## 15. Threads, safety, and exceptions

- Only one process can own the device USB interface at a time.
- Use one thread as the `Client` I/O owner.
- Do not call `readFrame()` or `Client` commands concurrently from multiple
  threads.
- `ImuStream` and `LidarStream` do not create threads; handlers run
  synchronously.
- An exception thrown by a handler propagates out of `handleFrame()`.
- Copy `VideoChunkView` data and stream-callback references before crossing a
  thread boundary.
- Put JPEG decoding, point-cloud rendering, and file writing in bounded worker
  queues.
- Before time synchronization, persistent configuration, LiDAR network, Wi-Fi,
  or upgrade operations, verify `streamTransferActive()==false`.
- Stream destructors attempt to stop and suppress stop exceptions; call
  `stop()` explicitly in normal control flow.
- USB removal, timeout, version mismatch, invalid parameters, and device-side
  rejection are reported as C++ exceptions.

Catch errors at the application boundary:

```cpp
try {
  runDevice();
} catch (const std::invalid_argument& error) {
  showConfigurationError(error.what());
} catch (const std::logic_error& error) {
  showStateError(error.what());
} catch (const std::exception& error) {
  showTransportOrProtocolError(error.what());
}
```

Do not continue using old stream wrappers after a `Client` disconnect. Destroy
them, enumerate again, and create a new `Client`.

<a id="sdk-windows-runtime"></a>
## 16. Windows Runtime API v5

Resolve the runtime entry point:

```cpp
using GetApi = prism::GetRuntimeApiFunction;
auto entry = reinterpret_cast<GetApi>(
    GetProcAddress(module, prism::kRuntimeApiEntryPoint));
if (entry == nullptr) {
  throw std::runtime_error("Prism Runtime API entry point is missing");
}
const prism::RuntimeApi* api = entry(prism::kRuntimeApiVersion);
if (api == nullptr) {
  throw std::runtime_error("Prism Runtime API v5 is unavailable");
}
```

Before making a call, verify:

- `abi_version == 4`;
- `struct_size >= sizeof(prism::RuntimeApi)`;
- `sdk_version == "1.0.0"`;
- `api->msvc_version / 100 == _MSC_VER / 100`, proving that the DLL and
  application use a compatible MSVC 14.x runtime family;
- every function pointer that the application will use is non-null.

Client lifecycle example:

```cpp
prism::Client* client = api->client_create();
if (client == nullptr) {
  throw std::runtime_error("cannot create Prism Client");
}
try {
  const auto devices =
      api->enumerate(prism::kDefaultVid, prism::kDefaultPid);
  if (devices.empty()) throw std::runtime_error("no device");
  api->open_device(client, devices.front());
  const auto info = api->device_info(client);
  api->close_device(client);
  api->client_destroy(client);
  client = nullptr;
} catch (...) {
  if (client != nullptr) api->client_destroy(client);
  throw;
}
```

Runtime API fields map to the direct API as follows:

| Runtime API | Equivalent feature |
| --- | --- |
| `client_create/destroy` | Construct/destroy Client |
| `enumerate/open_device/close_device/is_open/path/serial_number` | Device lifecycle |
| `keepalive_enabled/stream_transfer_active` | State queries |
| `hello/device_info/device_versions` | Versions and health |
| `synchronize_system_time` | Set and verify device system time |
| `network_info` | RK network information |
| `wifi_hotspot_status/set_wifi_hotspot_enabled` | Wi-Fi AP |
| `device_configuration/save_device_configuration` | Persistent configuration |
| `camera_exposure/set_exposure_configuration` | Runtime exposure |
| `camera_exposure_limits/set_camera_exposure_limits` | Automatic exposure and gain limits |
| `start_video/stop_video/start_imu/stop_imu/send_video_ack` | Camera/IMU |
| `start_lidar/stop_lidar/lidar_status` | LiDAR control |
| `lidar_network_status/save_lidar_network_configuration/probe_lidar_network` | LiDAR network |
| `read_frame` | The single receive loop |
| `inspect_system_upgrade_package/upgrade_system` | System upgrade |
| `parse_heartbeat` | Heartbeat |
| `parse_video_chunk_view/parse_video_meta/parse_imu_sample` | Camera/onboard IMU |
| `parse_lidar_point_batch/parse_lidar_imu_sample` | LiDAR data |
| `usb_link_speed_name/sensor_board_error_code_name` | Display helpers |

Windows does not provide `ImuStream` or `LidarStream` wrappers. Implement the
same dispatch described in Sections 8 through 11 with
`read_frame + switch + parser`. The `VideoChunkView::data` lifetime rule is
unchanged. Destroy every `Client` and every C++ object returned by the DLL
before calling `FreeLibrary()`.

<a id="sdk-build-integration"></a>
## 17. CMake integration and application distribution

Build every example supported on the current platform from the repository
root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

After copying `examples/` into a standalone project, point it at this binary
package with `PRISM_SDK_ROOT`:

```bash
cmake -S examples -B build-example \
  -DPRISM_SDK_ROOT=/absolute/path/to/Prism-SDK \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-example --config Release --parallel
```

Linux and macOS applications link the corresponding SDK library and should
preserve the example CMake's relative RPATH when distributing the executable.
macOS also requires `libusb-1.0.0.dylib`. Windows has only a DLL: use the
Runtime API loader from Section 16 and place `prism_usb_sdk.dll` beside the
executable. See the [installation guide](installation.md) for complete platform
dependencies and deployment requirements.
