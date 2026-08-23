# Prism Host SDK 逐接口示例

[English](interface-examples.md)

本手册为每个公开 SDK 接口提供具体例子。[快速目录](development-guide.zh-CN.md#api-quick-index)
中的每个接口都会跳转到实际使用它的例子。必须配套使用的调用会共用一个完整生命周期例子，
例如 start/read/stop 或 inspect/confirm/upgrade。

除非另有说明，直接 `Client`、Stream、helper 和 parser 示例适用于 Linux x86-64 与
macOS arm64，并假设已经包含：

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

修改持久配置、时钟、网络或固件的操作都由显式布尔变量保护。设置该变量前必须获得用户
确认，并继续遵守流状态和 idle-only 限制。

本手册中的每组片段也都有对应的、经过编译检查的源码目录：

- [Client 生命周期与基础控制](../examples/client_api_examples.cpp)
- [配置、采集、网络与升级](../examples/configuration_api_examples.cpp)
- [高级 Stream 封装](../examples/stream_api_examples.cpp)
- [helper 与全部公共解析器](../examples/parser_api_examples.cpp)
- [Windows Runtime API v5 全部 45 个入口](../examples/windows_runtime_api_examples.cpp)

GitHub Actions 会在每个受支持平台上编译该平台适用的全部目录程序。

## Client 生命周期与基础控制

<a id="example-client-construction"></a>
### 默认构造与 move

```cpp
prism::Client first;
prism::Client second = std::move(first);

prism::Client third;
third = std::move(second);
```

`Client` 可以 move，但不能 copy。被 move 的对象只能析构或重新赋值。持有对象离开
作用域时，`~Client()` 会自动关闭已打开设备；如果需要处理关闭错误，应显式调用 `close()`。

<a id="example-device-enumeration"></a>
### 枚举并打开指定设备

```cpp
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("no Prism device found");
}

auto client = prism::Client::open(devices.front());
// 单设备应用也可以使用：
// auto client = prism::Client::openFirst();
```

使用枚举结果中的 `DeviceInfo::serial_number` 选择指定 USB 设备。两个静态 factory
都会执行严格的 SDK/Agent 版本握手。

<a id="example-client-lifecycle"></a>
### 复用一个 Client 对象

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
client.close();  // close() 和 closeDevice() 是等价的公开操作。
```

<a id="example-keepalive"></a>
### 启用、查询和禁用 keepalive

```cpp
client.setKeepaliveEnabled(true, 1000);
if (!client.keepaliveEnabled()) {
  throw std::runtime_error("keepalive was not enabled");
}

client.setKeepaliveEnabled(false);
```

Keepalive 与其他控制命令共用命令通路，不要从另一个线程发出未协调的命令。

<a id="example-device-information"></a>
### 读取版本、健康、时钟、Ping 和网络信息

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

`deviceInfo()` 返回设备健康状态的新快照；枚举结果只包含 USB 身份字段。

<a id="example-time-sync"></a>
### 测量时间并选择是否同步设备

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

测量接口不会修改任何时钟。系统同步会修改 RK system/PTP/RTC 时间，因此必须先确认主机
时钟正确。

<a id="example-wifi"></a>
### 读取并显式修改 Wi-Fi 热点状态

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

修改热点可能立即断开网络用户；该操作仍通过 USB 控制通路执行。

## 配置、采集和升级

<a id="example-device-configuration"></a>
### 读取并选择性保存持久配置

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

速率写入只允许在 idle 状态执行；field mask 可避免覆盖其他无关设置。

<a id="example-exposure"></a>
### 读取并修改运行时曝光和 gain

```cpp
prism::ExposureConfiguration exposure = client.cameraExposure();
prism::ExposureLimits limits = client.cameraExposureLimits();
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

limits.min_exposure_time_us = 500;
limits.max_exposure_time_us = 20000;
limits.min_gain_x1024 = 1024;
limits.max_gain_x1024 = 8192;
limits = client.setCameraExposureLimits(
    limits, prism::kExposureLimitsFieldAll);
```

采集中应根据当前 `VideoStatus::fps` 计算上限，而不是使用持久 FPS。返回的
`effective_max_exposure_time_us` 是配置上限按当前 FPS 钳制后的实际值。曝光设置和
上下限只在运行时生效，不会持久保存。

<a id="example-camera-imu-control"></a>
### 启动和停止 Camera/IMU 聚合会话

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

任意一个 stop 接口都会停止共享的 Camera/IMU 采集会话。

<a id="example-video-ack"></a>
### 归还 Camera 流控 credit

```cpp
const uint32_t complete_frame_id = assembled_frame.frame_id;
if (assembled_frame.has_all_cameras && assembled_frame.has_metadata) {
  consume(assembled_frame);
  client.sendVideoAck(complete_frame_id);
}
```

只有收到全部 JPEG 和匹配 metadata 后才能 ACK。如果更新的 frame ID 证明旧帧分块不会
再到达，应丢弃并 ACK 旧残帧。完整实现见
[`camera_imu_capture.cpp`](../examples/camera_imu_capture.cpp)。

<a id="example-lidar-control"></a>
### 启动、查询和停止 LiDAR

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

型号参数必须提供；Mid360S 设备应使用 `Mid360S`。

<a id="example-lidar-network"></a>
### 检查、保存和探测 LiDAR 网络

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

`probeLidarNetwork()` 使用已保存的配置执行连通性测试；要测试输入的新值，必须先保存。

<a id="example-low-level"></a>
### 读取原始 Frame 并执行原始命令

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

每个 Client 只能有一个接收循环。除非实现 stream frame dispatcher，否则优先使用高级接口。

<a id="example-system-upgrade"></a>
### 检查并显式执行系统升级

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

低级升级状态帧只能按匹配类型调用 `parseUpgradeStatus(frame)` 或
`parseSensorBoardUpgradeStatus(frame)`。升级期间必须保持供电和 USB 稳定。

## Stream 包装类

<a id="example-imu-stream"></a>
### ImuStream 生命周期

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

必须从唯一接收线程把每个 Frame 交给 `handleFrame()`。`~ImuStream()` 会尽力停止，
但生产代码应显式调用 `stop()`，以便报告停止错误。

<a id="example-lidar-stream"></a>
### 仅点云与点云加 IMU 的 LidarStream 生命周期

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

同一时间只能运行其中一个生命周期。`~LidarStream()` 也会尝试停止活动数据流，但显式
`stop()` 才能返回错误。

## Helper 和 parser

<a id="example-host-version"></a>
### 查询 Host SDK 版本

```cpp
const std::string sdk_version = prism::hostSdkVersion();
std::cout << sdk_version << '\n';
```

<a id="example-device-name-helpers"></a>
### 将设备枚举值转换为名称

```cpp
const auto info = client.deviceInfo();
std::cout << prism::usbLinkSpeedName(info.usb_speed) << '\n';
for (const auto reason : info.imu_init_error_reason) {
  std::cout << prism::imuInitErrorReasonName(reason) << '\n';
}
std::cout << prism::sensorBoardErrorCodeName(info.sensor_board_error_code)
          << '\n';
```

返回的 `const char*` 是由 SDK 管理的静态字符串。

<a id="example-parser-dispatch"></a>
### 分发全部公开 telemetry parser

```cpp
void parse_frame(const prism::Frame& frame) {
  switch (frame.type) {
    case prism::FrameType::DeviceInfoResponse:
      consume(prism::parseDeviceInfo(frame));
      break;
    case prism::FrameType::ExposureResponse:
      consume(prism::parseExposureConfiguration(frame));
      break;
    case prism::FrameType::ExposureLimitsResponse:
      consume(prism::parseExposureLimits(frame));
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

严格 parser 会在 frame type、协议版本或 payload 大小错误时抛异常。`VideoChunkView`
只在源 `Frame` 存活时有效；`VideoChunk` 拥有自己的字节 vector。

## Windows Runtime API v5

<a id="example-windows-runtime"></a>
### 加载表并调用全部 45 个函数指针接口

完整可编译的 Windows loader 位于
[`device_info_time_sync.cpp`](../examples/device_info_time_sync.cpp)。程序加载
`prism_usb_sdk.dll`、解析 `prism_usb_sdk_get_runtime_api`、验证 ABI v5、SDK
1.0.0 和 MSVC 兼容性，并把结果保存到 `api` 后，全部函数指针的最小形式如下：

| Runtime API 字段 | 对应例子 |
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
| `camera_exposure_limits` | `auto value = api->camera_exposure_limits(client);` |
| `set_camera_exposure_limits` | `auto value = api->set_camera_exposure_limits(client, limits, field_mask);` |

使用前必须检查每个函数指针非空。直到所有 SDK 返回对象和 Client 都销毁后才能卸载 DLL。
Runtime API v5 是 Linux/macOS 直接 API 的子集；表中不存在的接口无法通过当前 Windows DLL
调用。
