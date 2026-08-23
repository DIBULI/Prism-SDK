# Prism Host SDK 1.0.0 开发手册

本文面向只获得公共头文件和预编译动态库的应用开发者，说明 Prism Host SDK
`1.0.0` 的全部公开功能、生命周期、数据单位、时间戳语义和使用约束。设备端
Agent 必须为 `1.0.0`，线协议必须为 `1`；SDK 在打开设备时执行严格版本校验，
不提供旧协议兼容模式。

统一头文件：

```cpp
#include <prism/usb_sdk.hpp>
```

所有公开类型均位于 `prism` 命名空间。更小的功能头文件位于
`include/prism/usb/`。

[English](development-guide.md)

下文的短代码片段专注于对应 SDK 调用，可能省略标准库头文件、业务回调、
命令行解析和停机信号。仓库 `examples/` 下的程序才是可独立编译的完整示例。

<a id="api-quick-index"></a>
## 逐接口最小 Example 与快速目录

本目录用于快速确认每个公开入口的调用形式。表内假设 `client` 已打开、`frame` 类型
正确；会改变设备的调用仍必须遵守后文的 idle、用户确认和回滚要求。点击“详细说明”
可直接跳转到对应章节。

- [Client 生命周期和基础控制](#api-client-control)
- [配置、采集和升级](#api-config-capture-update)
- [Stream 包装类](#api-stream-wrappers)
- [Free helper 和 parser](#api-helpers-parsers)
- [API 与头文件索引](#sdk-header-index)
- [Windows Runtime API v5](#sdk-windows-runtime)

<a id="api-client-control"></a>
### Client 生命周期和基础控制

| 接口 | 最小调用 | 详细说明 | 对应示例 |
| --- | --- | --- | --- |
| 默认构造 | `prism::Client client;` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-construction) |
| `Client` 析构 | `{ prism::Client client = prism::Client::openFirst(); }` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-construction) |
| move 构造/赋值 | `prism::Client next = std::move(client);` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-construction) |
| `enumerate` | `auto devices = prism::Client::enumerate();` | [设备枚举与选择](#sdk-device-open) | [示例](interface-examples.zh-CN.md#example-device-enumeration) |
| `openFirst` | `auto client = prism::Client::openFirst();` | [设备枚举与选择](#sdk-device-open) | [示例](interface-examples.zh-CN.md#example-device-enumeration) |
| `open` | `auto client = prism::Client::open(devices.front());` | [设备枚举与选择](#sdk-device-open) | [示例](interface-examples.zh-CN.md#example-device-enumeration) |
| `openFirstDevice` | `client.openFirstDevice();` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-lifecycle) |
| `openDevice` | `client.openDevice(devices.front());` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-lifecycle) |
| `closeDevice` | `client.closeDevice();` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-lifecycle) |
| `close` | `client.close();` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-lifecycle) |
| `isOpen` | `bool opened = client.isOpen();` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-lifecycle) |
| `path` | `std::wstring usb_path = client.path();` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-lifecycle) |
| `serialNumber` | `std::wstring usb_serial = client.serialNumber();` | [Client 生命周期](#sdk-client-lifecycle) | [示例](interface-examples.zh-CN.md#example-client-lifecycle) |
| `setKeepaliveEnabled` | `client.setKeepaliveEnabled(true, 1000);` | [Keepalive](#sdk-keepalive) | [示例](interface-examples.zh-CN.md#example-keepalive) |
| `keepaliveEnabled` | `bool keepalive = client.keepaliveEnabled();` | [Keepalive](#sdk-keepalive) | [示例](interface-examples.zh-CN.md#example-keepalive) |
| `hello` | `auto hello = client.hello();` | [HELLO 与固件版本](#sdk-hello-versions) | [示例](interface-examples.zh-CN.md#example-device-information) |
| `deviceInfo` | `auto info = client.deviceInfo();` | [设备健康信息](#sdk-device-info) | [示例](interface-examples.zh-CN.md#example-device-information) |
| `deviceVersions` | `auto versions = client.deviceVersions();` | [HELLO 与固件版本](#sdk-hello-versions) | [示例](interface-examples.zh-CN.md#example-device-information) |
| `boardTime` | `auto rk_time = client.boardTime();` | [时间、Ping 与网络](#sdk-time-network) | [示例](interface-examples.zh-CN.md#example-device-information) |
| `synchronizeTimeNtpLike` | `auto measured = client.synchronizeTimeNtpLike(12, 1000);` | [时间同步](#sdk-time-sync) | [示例](interface-examples.zh-CN.md#example-time-sync) |
| `synchronizeSystemTime` | `auto synced = client.synchronizeSystemTime(12, 6, 1000);` | [时间同步](#sdk-time-sync) | [示例](interface-examples.zh-CN.md#example-time-sync) |
| `streamTransferActive` | `bool active = client.streamTransferActive();` | [线程与安全约束](#sdk-thread-safety) | [示例](interface-examples.zh-CN.md#example-time-sync) |
| `ping` | `uint64_t board_seq = client.ping();` | [时间、Ping 与网络](#sdk-time-network) | [示例](interface-examples.zh-CN.md#example-device-information) |
| `networkInfo` | `auto network = client.networkInfo();` | [时间、Ping 与网络](#sdk-time-network) | [示例](interface-examples.zh-CN.md#example-device-information) |
| `wifiHotspotStatus` | `auto ap = client.wifiHotspotStatus();` | [Wi-Fi 热点管理](#sdk-wifi) | [示例](interface-examples.zh-CN.md#example-wifi) |
| `setWifiHotspotEnabled` | `auto ap = client.setWifiHotspotEnabled(true);` | [Wi-Fi 热点管理](#sdk-wifi) | [示例](interface-examples.zh-CN.md#example-wifi) |

<a id="api-config-capture-update"></a>
### 配置、采集和升级

| 接口 | 最小调用 | 详细说明 | 对应示例 |
| --- | --- | --- | --- |
| `deviceConfiguration` | `auto cfg = client.deviceConfiguration();` | [持久化设备配置](#sdk-configuration) | [示例](interface-examples.zh-CN.md#example-device-configuration) |
| `saveDeviceConfiguration` | `cfg = client.saveDeviceConfiguration(cfg, prism::kDeviceConfigFieldCameraFps);` | [持久化设备配置](#sdk-configuration) | [示例](interface-examples.zh-CN.md#example-device-configuration) |
| `cameraExposure` | `auto exposure = client.cameraExposure();` | [曝光和增益](#sdk-exposure) | [示例](interface-examples.zh-CN.md#example-exposure) |
| `setExposureConfiguration` | `exposure = client.setExposureConfiguration(exposure, prism::kExposureFieldAll);` | [曝光和增益](#sdk-exposure) | [示例](interface-examples.zh-CN.md#example-exposure) |
| `cameraExposureLimits` | `auto limits = client.cameraExposureLimits();` | [曝光和增益](#sdk-exposure) | [示例](interface-examples.zh-CN.md#example-exposure) |
| `setCameraExposureLimits` | `limits = client.setCameraExposureLimits(limits, prism::kExposureLimitsFieldAll);` | [曝光和增益](#sdk-exposure) | [示例](interface-examples.zh-CN.md#example-exposure) |
| `setAutoExposureTargetBrightness` | `client.setAutoExposureTargetBrightness(35);` | [曝光和增益](#sdk-exposure) | [示例](interface-examples.zh-CN.md#example-exposure) |
| `setCameraExposure` | `client.setCameraExposure(0, camera);` | [曝光和增益](#sdk-exposure) | [示例](interface-examples.zh-CN.md#example-exposure) |
| `startVideo1280x1024` | `auto video = client.startVideo1280x1024(0);` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-camera-imu-control) |
| `stopVideo` | `client.stopVideo();` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-camera-imu-control) |
| `startImu` | `auto imu_status = client.startImu(info.detected_imu_count, 0);` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-camera-imu-control) |
| `stopImu` | `auto imu_status = client.stopImu();` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-camera-imu-control) |
| `sendVideoAck` | `client.sendVideoAck(last_complete_frame_id);` | [JPEG 重组与 ACK](#sdk-video-ack) | [示例](interface-examples.zh-CN.md#example-video-ack) |
| `startLidar` | `auto lidar = client.startLidar(prism::LidarModel::Mid360);` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-control) |
| `stopLidar` | `auto lidar = client.stopLidar();` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-control) |
| `lidarStatus` | `auto lidar = client.lidarStatus();` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-control) |
| `lidarNetworkStatus` | `auto net = client.lidarNetworkStatus();` | [LiDAR 网络](#sdk-lidar-network) | [示例](interface-examples.zh-CN.md#example-lidar-network) |
| `saveLidarNetworkConfiguration` | `auto net = client.saveLidarNetworkConfiguration(configuration);` | [LiDAR 网络](#sdk-lidar-network) | [示例](interface-examples.zh-CN.md#example-lidar-network) |
| `probeLidarNetwork` | `auto net = client.probeLidarNetwork();` | [LiDAR 网络](#sdk-lidar-network) | [示例](interface-examples.zh-CN.md#example-lidar-network) |
| `readFrame` | `prism::Frame frame = client.readFrame(3000);` | [低级 Frame 与命令](#sdk-low-level) | [示例](interface-examples.zh-CN.md#example-low-level) |
| `command` | `auto pong = client.command(prism::FrameType::Ping);` | [低级 Frame 与命令](#sdk-low-level) | [示例](interface-examples.zh-CN.md#example-low-level) |
| `upgradeSystem` | `auto result = client.upgradeSystem(package_path, options, progress);` | [系统升级](#sdk-system-upgrade) | [示例](interface-examples.zh-CN.md#example-system-upgrade) |

<a id="api-stream-wrappers"></a>
### Stream 包装类

| 接口 | 最小调用 | 详细说明 | 对应示例 |
| --- | --- | --- | --- |
| `ImuStream` 构造 | `prism::ImuStream imu(client, on_imu);` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-imu-stream) |
| `ImuStream` 析构 | `{ prism::ImuStream imu(client, on_imu); }` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-imu-stream) |
| `ImuStream::start` | `imu.start(info.detected_imu_count, 0);` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-imu-stream) |
| `ImuStream::handleFrame` | `bool consumed = imu.handleFrame(frame);` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-imu-stream) |
| `ImuStream::active` | `bool active = imu.active();` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-imu-stream) |
| `ImuStream::stop` | `imu.stop();` | [Camera/IMU 采集](#sdk-camera-imu) | [示例](interface-examples.zh-CN.md#example-imu-stream) |
| `LidarStream` 点云构造 | `prism::LidarStream lidar(client, on_points);` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-stream) |
| `LidarStream` 双回调构造 | `prism::LidarStream lidar(client, on_points, on_lidar_imu);` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-stream) |
| `LidarStream` 析构 | `{ prism::LidarStream lidar(client, on_points); }` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-stream) |
| `LidarStream::start` | `lidar.start(prism::LidarModel::Mid360);` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-stream) |
| `LidarStream::handleFrame` | `bool consumed = lidar.handleFrame(frame);` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-stream) |
| `LidarStream::active` | `bool active = lidar.active();` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-stream) |
| `LidarStream::stop` | `lidar.stop();` | [LiDAR 数据流](#sdk-lidar-stream) | [示例](interface-examples.zh-CN.md#example-lidar-stream) |

<a id="api-helpers-parsers"></a>
### Free helper 和 parser

| 接口 | 最小调用 | 详细说明 | 对应示例 |
| --- | --- | --- | --- |
| `hostSdkVersion` | `std::string version = prism::hostSdkVersion();` | [SDK 版本](#sdk-version) | [示例](interface-examples.zh-CN.md#example-host-version) |
| `frameTypeName` | `std::string name = prism::frameTypeName(frame.type);` | [低级 Frame 与命令](#sdk-low-level) | [示例](interface-examples.zh-CN.md#example-low-level) |
| `isCameraFpsSupported` | `bool supported = prism::isCameraFpsSupported(30);` | [持久化设备配置](#sdk-configuration) | [示例](interface-examples.zh-CN.md#example-device-configuration) |
| `cameraMaxExposureUs` | `uint32_t limit = prism::cameraMaxExposureUs(30);` | [曝光和增益](#sdk-exposure) | [示例](interface-examples.zh-CN.md#example-exposure) |
| `usbLinkSpeedName` | `const char* name = prism::usbLinkSpeedName(info.usb_speed);` | [设备健康信息](#sdk-device-info) | [示例](interface-examples.zh-CN.md#example-device-name-helpers) |
| `imuInitErrorReasonName` | `const char* name = prism::imuInitErrorReasonName(reason);` | [设备健康信息](#sdk-device-info) | [示例](interface-examples.zh-CN.md#example-device-name-helpers) |
| `sensorBoardErrorCodeName` | `const char* name = prism::sensorBoardErrorCodeName(code);` | [设备健康信息](#sdk-device-info) | [示例](interface-examples.zh-CN.md#example-device-name-helpers) |
| `inspectSystemUpgradePackage` | `auto package = prism::inspectSystemUpgradePackage(package_path);` | [系统升级](#sdk-system-upgrade) | [示例](interface-examples.zh-CN.md#example-system-upgrade) |
| `parseDeviceInfo` | `auto value = prism::parseDeviceInfo(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseExposureConfiguration` | `auto value = prism::parseExposureConfiguration(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseExposureLimits` | `auto value = prism::parseExposureLimits(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseWifiHotspotStatus` | `auto value = prism::parseWifiHotspotStatus(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseHeartbeat` | `auto value = prism::parseHeartbeat(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseVideoChunkView` | `auto view = prism::parseVideoChunkView(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseVideoChunk` | `auto owned = prism::parseVideoChunk(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseVideoMeta` | `auto value = prism::parseVideoMeta(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseImuSample` | `auto value = prism::parseImuSample(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseLidarStatus` | `auto value = prism::parseLidarStatus(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseLidarNetworkStatus` | `auto value = prism::parseLidarNetworkStatus(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseLidarPointBatch` | `auto value = prism::parseLidarPointBatch(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseLidarImuSample` | `auto value = prism::parseLidarImuSample(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-parser-dispatch) |
| `parseUpgradeStatus` | `auto value = prism::parseUpgradeStatus(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-system-upgrade) |
| `parseSensorBoardUpgradeStatus` | `auto value = prism::parseSensorBoardUpgradeStatus(frame);` | [全部 parser](#sdk-parsers) | [示例](interface-examples.zh-CN.md#example-system-upgrade) |

Windows Runtime API v5 对应函数指针的最小形式是 `api->field(client, ...)`；全部 45 个字段
已在 [Windows Runtime API v5](#sdk-windows-runtime) 中按直接 API 分组映射。

<a id="sdk-header-index"></a>
## API 索引

| 头文件 | 公开功能 |
| --- | --- |
| `client.hpp` | Client 生命周期、控制、采集、低级 Frame、升级 |
| `common.hpp` | 常量、enum、DeviceInfo、Frame、HELLO、版本 helper |
| `configuration.hpp` | 持久配置、字段 mask、FPS/quality 范围 |
| `device_info.hpp` | DeviceInfo parser 和状态名称 helper |
| `exposure.hpp` | 运行时曝光、gain、范围和 parser |
| `streams.hpp` | `ImuStream`、`LidarStream` 和回调类型 |
| `telemetry.hpp` | Camera、IMU、LiDAR、网络、heartbeat 数据与 parser |
| `time_sync.hpp` | 时间测量与设置结果 |
| `update.hpp` | 完整系统升级包、进度、状态与 parser |
| `wifi.hpp` | Wi‑Fi AP 状态与 parser |
| `runtime_api.hpp` | Windows Runtime API v5 |

可编译的完整示例：

- [跨平台设备信息/时间同步](../examples/device_info_time_sync.cpp)；
- [Camera/IMU 采集、JPEG 重组和 ACK](../examples/camera_imu_capture.cpp)；
- [LiDAR 点云与 LiDAR IMU 统计](../examples/lidar_capture.cpp)；
- [Client 生命周期与基础控制 API 目录](../examples/client_api_examples.cpp)；
- [配置、曝光、采集、网络与升级 API 目录](../examples/configuration_api_examples.cpp)；
- [高级 Stream API 目录](../examples/stream_api_examples.cpp)；
- [helper 与 parser API 目录](../examples/parser_api_examples.cpp)；
- [Windows Runtime API v5 目录](../examples/windows_runtime_api_examples.cpp)。

每个公开 Client 操作、Stream 接口、parser 和 Runtime API v5 函数指针，都可以从上方
快速目录跳转到对应示例和下方详细章节。8 个源文件按功能类别集中，避免为每个便利重载
复制一个几乎相同的 executable；GitHub Actions 平台矩阵会编译全部源文件。

<a id="sdk-platform-models"></a>
## 1. 平台与 API 模型

| 平台 | 架构 | 使用方式 |
| --- | --- | --- |
| Ubuntu 22.04+ | x86-64 或 arm64 | 链接 `libprism_usb_sdk.so` 或 `libprism_usb_sdk.a`，使用完整 `Client` API |
| macOS 13+ | Apple Silicon arm64 | 链接 SDK dylib，并随程序部署 libusb dylib，使用完整 `Client` API |
| Windows 10/11 | x64、MSVC 14.x | `LoadLibraryExW` 加载 DLL，通过 Runtime API v5 调用 |

Windows 发布包不包含 import library，不能直接链接 `Client` 成员函数。完整、安全的
DLL 加载流程见
[`examples/device_info_time_sync.cpp`](../examples/device_info_time_sync.cpp)。

Windows Runtime API v5 暴露大部分常用控制、采集和解析功能，但不暴露以下接口：

- `boardTime()`、`ping()`；
- `synchronizeTimeNtpLike()`；
- `setKeepaliveEnabled()`；
- `setAutoExposureTargetBrightness()`、`setCameraExposure()` 便利函数；
- `ImuStream`、`LidarStream` 包装类；
- `hostSdkVersion()`（改读 `api->sdk_version`）、`command()`、`frameTypeName()`；
- `imuInitErrorReasonName()`、`parseDeviceInfo()`、owning `parseVideoChunk()`、
  `parseLidarStatus()`、`parseLidarNetworkStatus()`、曝光/Wi-Fi/升级状态 parser。

Windows 可使用函数表中的通用读写、`read_frame` 和 parser 实现等价业务；本手册的
直接 `Client` 代码默认针对 Linux/macOS。第 16 节单独说明 Windows 映射。

## 2. 最小生命周期

<a id="sdk-version"></a>
### 2.1 查询 SDK 版本

```cpp
std::cout << prism::hostSdkVersion() << '\n';  // 1.0.0
```

<a id="sdk-device-open"></a>
### 2.2 枚举并按序列号选择设备

```cpp
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("未发现 Prism 设备");
}

const std::wstring wanted = L"1b78953e9fc85455";
const auto it = std::find_if(
    devices.begin(), devices.end(), [&](const prism::DeviceInfo& device) {
      return device.serial_number == wanted;
    });
if (it == devices.end()) {
  throw std::runtime_error("指定序列号未连接");
}
auto client = prism::Client::open(*it);
```

`enumerate(vid, pid)` 可覆盖默认 `0x2207:0x1201`。枚举结果只填写 USB 身份字段：
`path`、`serial_number`、`vendor_id` 和 `product_id`。`path` 可能随插拔变化，应用应以
`serial_number` 作为稳定选择标识。

单设备程序可简写：

```cpp
auto client = prism::Client::openFirst();
```

<a id="sdk-client-lifecycle"></a>
### 2.3 复用一个 Client 对象

```cpp
prism::Client client;
client.openFirstDevice();
std::wcout << client.path() << L' ' << client.serialNumber() << L'\n';
if (!client.isOpen()) throw std::runtime_error("设备未打开");
client.closeDevice();
```

也可用 `openDevice(device)` 和 `close()`；`close()` 与 `closeDevice()` 等价。已打开时
再次打开会抛出 `std::logic_error`。`Client` 可移动但不可复制；所有 Stream 包装类
必须比其引用的 `Client` 更早销毁。

## 3. 版本、健康与基础信息

<a id="sdk-hello-versions"></a>
### 3.1 HELLO 与固件版本

```cpp
const prism::HelloInfo hello = client.hello();
const prism::DeviceVersions versions = client.deviceVersions();

std::cout << hello.app << ' ' << hello.version << '\n'
          << "protocol=" << hello.protocol_version << '\n'
          << "agent=" << versions.agent << '\n'
          << "sensor-board=" << versions.sensor_board << '\n'
          << versions.combined << '\n';
```

`hello()` 还返回最大负载、Agent PID、进程启动单调时间和上一次升级结果。
`process_id + process_started_monotonic_us` 可用于识别 Agent 是否重启。

<a id="sdk-device-info"></a>
### 3.2 DeviceInfo 新鲜状态快照

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

`deviceInfo()` 可在 Camera/IMU 数据流活动时获取新鲜健康快照。此时专用的
`wifiHotspotStatus()` 不可调用，但可读取 `info.wifi` 展示 AP 状态。为避免与唯一
USB 接收循环竞争，仍应由同一 I/O owner 线程串行查询。

开始采集前建议检查：

- Camera 使用 `usb3_connected`；USB2 通常不足以保证四路高帧率；
- `sensor_board_online`；
- 需要传感器同步时间时检查 `sensor_board_time_synced`；
- `camera_present_mask`、`imu_present_mask` 和实际检测数量；
- `imu_init_error_mask == 0`；
- `sensor_board_error_flags == 0`。

不要写死两颗板载 IMU。请求数量应来自 `detected_imu_count`，有效范围是 1..2。

`usbLinkSpeedName()`、`imuInitErrorReasonName()`、
`sensorBoardErrorCodeName()` 只用于显示；业务判断应使用 enum 值。

<a id="sdk-time-network"></a>
### 3.3 时间、Ping 与网络信息

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

打开设备后 SDK 默认每 1000 ms 自动发送 keepalive。生产程序应保持启用。

```cpp
std::cout << client.keepaliveEnabled() << '\n';
client.setKeepaliveEnabled(true, 1000);  // 允许 100..4000 ms
```

`setKeepaliveEnabled(false)` 主要用于看门狗测试。长时间不发 keepalive 时 Agent 会停止
采集，不能把它当作普通节能功能。

<a id="sdk-configuration"></a>
## 4. 持久化设备配置

```cpp
prism::DeviceConfiguration config = client.deviceConfiguration();
std::cout << config.camera_fps << ' '
          << config.imu_rate_hz << ' '
          << config.mjpeg_quality << '\n';
```

支持范围：

- Camera FPS：任意整数 `1..30`；可先调用 `isCameraFpsSupported(fps)`；
- IMU rate：`500` 或 `1000` Hz；
- MJPEG quality：`1..99`，默认 `88`。

局部保存并回读：

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

if (!config.persisted) throw std::runtime_error("配置未持久化");
```

同时更新三个字段可使用 `kDeviceConfigFieldAll`。`generation` 是设备维护的修订号。
持久配置写入要求 Camera、板载 IMU 和 LiDAR 全部停止；新 MJPEG quality 在下一次
启动 Camera 管线时生效。

`startVideo1280x1024(0)` 和 `startImu(count, 0)` 中的 0 表示使用已保存值；非零值只
覆盖当前采集会话。

<a id="sdk-exposure"></a>
## 5. 运行时曝光和增益

曝光配置不持久化，Agent 或 sensor-board 重启后恢复默认值；允许在采集期间读写。

### 5.1 读取全部四路状态

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

### 5.2 调整自动曝光目标亮度

```cpp
client.setAutoExposureTargetBrightness(35);  // 允许 1..255
```

目标亮度为四路共用参数。

### 5.3 单路手动曝光和增益

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

硬件范围为曝光 50..995000 us，gain 1024..126976（1x..124x），步进 32。
当前运行时配置可以进一步缩小该范围；实际最高曝光时间始终不会超过
`floor(1000000 / fps) - 5000` us。

恢复单路自动模式：

```cpp
camera.mode = prism::CameraExposureMode::Automatic;
client.setCameraExposure(0, camera);
```

### 5.4 一次写入全部字段

```cpp
auto all = client.cameraExposure();
all.target_brightness = 40;
all.automatic_camera_mask = 0x0f;
all.gain_x1024.fill(1024);
all = client.setExposureConfiguration(all, prism::kExposureFieldAll);
```

字段掩码支持 `kExposureFieldTargetBrightness`、Camera 0..3 单独 bit、
`kExposureFieldCameraAll` 和 `kExposureFieldAll`。

### 5.5 配置自动曝光和 gain 上下限

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

四路相机共用这组运行时限制，设备重启后恢复默认。自动增亮时先增加曝光，达到
实际曝光上限后才提高 gain；自动减亮时先降低 gain，再减少曝光。Agent 会把已有
手动值钳制进新范围。如果新的 FPS 无法满足最低曝光时间，FPS 修改会被拒绝。

<a id="sdk-time-sync"></a>
## 6. 时间测量与系统时间同步

两种接口均要求所有数据流停止。

### 6.1 只测量，不修改时钟

```cpp
if (client.streamTransferActive()) {
  throw std::logic_error("先停止所有数据流");
}
const prism::NtpTimeSyncResult measured =
    client.synchronizeTimeNtpLike(12, 1000);
std::cout << "device_minus_host_us=" << measured.offset_us << '\n'
          << "round_trip_us=" << measured.round_trip_us << '\n'
          << "jitter_us=" << measured.jitter_us << '\n';
```

`TimeSyncSample` 保存每次交换的四个时间戳、往返时间和偏差。

### 6.2 以主机时间校准设备

```cpp
if (client.streamTransferActive()) {
  throw std::logic_error("先停止所有数据流");
}
// 仅在用户显式选择“同步时间”且主机时钟准确后执行。
const prism::SystemTimeSyncResult result =
    client.synchronizeSystemTime(12, 6, 1000);
std::cout << "before=" << result.before.offset_us << '\n'
          << "applied=" << result.applied_correction_us << '\n'
          << "after=" << result.after.offset_us << '\n'
          << "verified=" << result.verified << '\n';
```

该操作修改 RK `CLOCK_REALTIME`、Ethernet PTP 硬件时钟和 RK RTC，不修改主机时钟，
也不替代 sensor-board 的 GPS/NMEA+PPS 同步。主机时间不准确时禁止调用。验证失败会
抛异常；正常返回时 `verified` 应为 true。

`sample_count` 和 `verification_sample_count` 的允许范围均为 3..64，
`timeout_ms` 为 100..10000。

<a id="sdk-wifi"></a>
## 7. Wi‑Fi 热点管理

Wi‑Fi API 只管理设备 AP，不提供 Wi‑Fi 传感器数据传输。查询和修改均要求数据流停止。

```cpp
const prism::WifiHotspotStatus status = client.wifiHotspotStatus();
std::cout << status.present << ' '
          << status.enabled << ' '
          << status.running << ' '
          << status.ssid << ' '
          << status.address << '\n';
```

显式开关并检查设备端错误：

```cpp
const auto changed = client.setWifiHotspotEnabled(true);
if (changed.error_code != 0) {
  throw std::runtime_error(changed.error);
}
```

`enabled` 表示持久化策略；`running` 只有 AP 和 DHCP 都运行时才为 true。
`present=false` 不是协议错误，应用仍可展示状态。

<a id="sdk-camera-imu"></a>
## 8. Camera 与板载 IMU 聚合采集

Camera 和板载 IMU 属于同一个采集会话：`startVideo1280x1024()` 和 `startImu()` 都会
使两条路径活动；`stopVideo()` 或 `stopImu()` 任一个都会停止整个聚合会话。

推荐顺序：先启动 Camera，再使用实际检测数量确认 IMU：

```cpp
const auto info = client.deviceInfo();
if (info.detected_imu_count == 0) {
  throw std::runtime_error("没有可用板载 IMU");
}

const prism::VideoStatus video = client.startVideo1280x1024(0);
if (!video.enabled || video.cameras == 0 || video.width != 1280 ||
    video.height != 1024) {
  throw std::runtime_error("Camera 未按预期启动");
}
prism::ImuStream imu(client, [](const prism::ImuSample& sample) {
  // 回调在 handleFrame() 调用线程内同步执行。
  consumeImu(sample);
});
imu.start(info.detected_imu_count, 0);
```

`VideoStatus` 返回实际启用状态、相机数、FPS、宽高和负载大小。固定图像尺寸为
1280x1024，`fps=0` 使用持久配置，显式 FPS 支持任意整数 1..30。

### 8.1 唯一接收循环

所有数据共享一个 USB IN 端点，只能有一个 `readFrame()` 读取者：

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

`ImuStream::handleFrame()` 对 IMU 帧返回 true，对其他帧返回 false。`active()` 可查询
状态；重复 `start()`/`stop()` 是安全的。析构会尝试停止但吞掉停止异常，生产代码应
显式 `stop()`。

可独立编译的 Camera/IMU 完整接收、chunk 重组和 ACK 示例见
[`examples/camera_imu_capture.cpp`](../examples/camera_imu_capture.cpp)。

<a id="sdk-video-ack"></a>
### 8.2 JPEG chunk 重组与 ACK

`VideoChunkView` 是零拷贝视图，其 `data` 只在源 `Frame` 未销毁、移动或修改时有效。
跨线程或异步解码必须复制；需要拥有数据可直接用 `parseVideoChunk()`。

重组规则：

1. 以 `(camera_id, frame_id)` 为键；
2. 预分配 `encoded_size` 字节；
3. 将 `data_size` 字节写入 `chunk_offset`；
4. 校验范围、重复和重叠；
5. 四路 JPEG 和同一 `host_frame_id` 的 metadata 全部到达后，按连续 frame id ACK；
6. ACK 应在 JPEG 解码、渲染和写盘前发送。

```cpp
if (frameSetComplete(next_frame_id)) {
  client.sendVideoAck(next_frame_id);
  ++next_frame_id;
}
```

若有序 USB 流已进入更新 `frame_id`，旧残帧的 chunk 将不再到达；应将该旧帧显式标记为丢弃
并 ACK 它自身的 credit，然后继续保持 ACK ID 连续。不能只 ACK 更新 ID 来隐式越过旧帧，也不能
永久等待旧残帧。metadata 已到达即可用于 credit 完整性；`meta.valid=false` 的帧仍需 ACK，
但不得作为有效传感器数据消费。每次新 capture 都应清空重组状态。

<a id="sdk-camera-metadata"></a>
### 8.3 Camera metadata 与真实时间

```cpp
const prism::VideoMeta meta = prism::parseVideoMeta(frame);
const bool camera_time_synced =
    client.deviceInfo().sensor_board_time_synced;
if (meta.valid && camera_time_synced && meta.trigger_time_ns != 0) {
  const uint64_t camera_time_ns = meta.trigger_time_ns;
}
```

`trigger_time_ns` 是四路公共 TRIG0 触发沿，不是曝光中心、ISP 完成或 USB 到达时间。
`VideoMeta::valid` 只证明 metadata 结构有效，不证明时钟已同步；Camera 时间能用于对齐
还必须确认 `DeviceInfo::sensor_board_time_synced`。
`host_frame_id` 用来匹配 JPEG；`exposure_us`、`analog_gain_x1024` 和
`digital_gain_x1024` 是该帧实际值。

<a id="sdk-onboard-imu"></a>
### 8.4 板载 IMU 数据

`ImuSample` 单位：

- `accel_mg`：milli-g；
- `gyro_mdps`：milli-degree/s；
- `temp_milli_c`：milli-degree Celsius；
- `timestamp_us`：微秒。

```cpp
const double ax_m_s2 = sample.accel_mg[0] * 9.80665 / 1000.0;
const double gx_rad_s = sample.gyro_mdps[0] * 3.14159265358979323846 / 180000.0;
```

只有 `timestamp_synced=true`（或 `flags & kImuFlagTimestampSynced`）时才可与同步的
Camera/LiDAR 时间比较。使用 `(sensor_id, sample_id)` 检查连续性；
`fsync_event`、`fsync_delay_valid` 和 `sample_gap` 分别表示 FSYNC 后首个 ODR sample、
有效 delay 字段和检测到大于 4 个 ODR 的间隔。

停止聚合会话只调用一个 stop：

```cpp
imu.stop();  // 同时停止 Camera 和板载 IMU
```

<a id="sdk-lidar-network"></a>
## 9. LiDAR 网络

LiDAR 网络查询、保存和探测都是 idle-only 操作。

```cpp
const auto status = client.lidarNetworkStatus();
std::cout << status.interface_name << ' '
          << status.configuration.host_ip << ' '
          << status.configuration.lidar_ip << '\n';
```

保存 end0 配置：

```cpp
if (client.streamTransferActive()) {
  throw std::logic_error("先停止所有数据流");
}
const auto previous = client.lidarNetworkStatus();  // 保留以便回退
std::cout << "previous host IP=" << previous.configuration.host_ip << '\n';
// 仅在用户显式确认新的 IP/掩码后继续。
prism::LidarNetworkConfiguration network;
network.enabled = true;
network.host_ip = "192.168.1.5";
network.netmask = "255.255.255.0";
network.lidar_ip = "192.168.1.3";
const auto saved = client.saveLidarNetworkConfiguration(network);
if (!saved.persisted) throw std::runtime_error("LiDAR 网络配置未保存");
```

保存会改变 RK `end0` 的持久配置。应将旧值展示给用户并提供回退；不要在程序启动时
无条件写死默认地址。

执行设备端连接测试：

```cpp
const auto probe = client.probeLidarNetwork();
if (!probe.target_reachable) {
  std::cerr << probe.error << '\n';
}
```

`target_reachable=false` 且 `error_code=0` 表示尚未完成本次连接测试，不等同于失败。
`link_up`、`address_applied`、`same_subnet` 和 `target_reachable` 应分别展示。

<a id="sdk-lidar-stream"></a>
## 10. LiDAR 点云与 LiDAR IMU

型号必须由用户明确选择，Agent 不根据发现结果猜测：

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

lidar.start(prism::LidarModel::Mid360);  // 或 Mid360S
while (running) {
  const prism::Frame frame = client.readFrame(3000);
  if (lidar.handleFrame(frame)) continue;
  // 继续分发 Camera、板载 IMU和 heartbeat。
}
const prism::LidarStatus status = client.lidarStatus();
lidar.stop();
```

`LidarStream` 至少需要一个回调；回调同步运行在 `handleFrame()` 所在线程，不能在其中
做点云渲染、长时间写盘或其他阻塞工作。
只需要点云时可使用单回调重载 `LidarStream(client, point_handler)`；
`lidar.active()` 返回包装类的本地活动状态。可独立编译的点云与 LiDAR IMU 统计示例见
[`examples/lidar_capture.cpp`](../examples/lidar_capture.cpp)。

`LidarPoint` 坐标单位为 mm，包含 reflectivity 和原始 tag。每批点数可变，不要写死。
`timestamp_utc_us` 为批次基准时间；只有 `timestamp_synced=true` 时才可靠。
`time_interval_100ns` 是批内第一点到最后一点的总跨度，不是逐点间隔；SDK 不展开逐点
时间，也不 deskew。

`timestamp_raw`/`timestamp_raw_ns`、`time_type` 和 `tai_offset_applied` 保留 LiDAR
原始时间信息及归一化记录。常规融合应使用已同步的 `timestamp_utc_us`；不要在不了解
Livox `time_type` 和当前时钟源的情况下将 raw 值直接与 Camera/板载 IMU 比较。

`LidarImuSample` 已使用 SI：gyro 为 rad/s，accel 为 m/s²。它与板载 `ImuSample`
不是同一种结构和单位。

仅检查 `LidarStatus::receiving` 不足以判断点云健康，还应确认 `point_count` 持续增长、
batch 非空，并监控 `dropped_point_count`。可直接使用低级接口：

```cpp
client.startLidar(prism::LidarModel::Mid360);
const auto running_status = client.lidarStatus();
const auto stopped_status = client.stopLidar();
```

<a id="sdk-combined-streams"></a>
## 11. Camera、IMU、LiDAR 同时采集

三个数据源共用同一个 `readFrame()` 循环：

```cpp
auto client = prism::Client::openFirst();
const auto info = client.deviceInfo();
if (info.detected_imu_count == 0 || info.detected_imu_count > 2) {
  throw std::runtime_error("板载 IMU 数量无效");
}

prism::ImuStream imu(client, onBoardImu);
prism::LidarStream lidar(client, onPoints, onLidarImu);

const auto video_status = client.startVideo1280x1024(0);
if (!video_status.enabled || video_status.cameras == 0) {
  throw std::runtime_error("Camera 未启动");
}
imu.start(info.detected_imu_count, 0);
lidar.start(prism::LidarModel::Mid360);
if (!client.lidarStatus().enabled) {
  throw std::runtime_error("LiDAR 未启动");
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

清理顺序为 LiDAR、Camera/IMU 聚合会话、Stream 对象、Client。异常路径也要执行同样
清理。回调需要跨线程时复制必要数据并放入有界队列；队列满时必须有明确 drop 策略，
不能阻塞 USB reader。

统一为 ns 的同步时间字段：

```cpp
const uint64_t camera_ns = meta.trigger_time_ns;
const uint64_t board_imu_ns = sample.timestamp_us * 1000u;
const uint64_t lidar_ns = batch.timestamp_utc_us * 1000u;
const uint64_t lidar_imu_ns = lidar_imu.timestamp_utc_us * 1000u;
```

Camera 必须同时满足 `meta.valid`、`meta.trigger_time_ns != 0` 和
`info.sensor_board_time_synced`；板载 IMU、LiDAR 点云和 LiDAR IMU 分别检查各自的
`timestamp_synced`。不能用 `VideoChunk::timestamp_us` 代替 Camera 触发时间，也不能把未同步或
raw LiDAR 时间混入同一数据集。

<a id="sdk-low-level"></a>
## 12. Heartbeat、Frame 与低级命令

### 12.1 Heartbeat

```cpp
const auto frame = client.readFrame(3000);
if (frame.type == prism::FrameType::Heartbeat) {
  const auto heartbeat = prism::parseHeartbeat(frame);
  std::cout << heartbeat.rk_system_time_us << '\n';
}
```

Heartbeat 只包含 RK 墙钟，不包含完整设备健康；健康状态使用 `deviceInfo()`。

### 12.2 Frame 日志

```cpp
std::cout << prism::frameTypeName(frame.type)
          << " seq=" << frame.sequence
          << " payload=" << frame.payload.size() << '\n';
```

### 12.3 原始 command

```cpp
const prism::Frame pong = client.command(prism::FrameType::Ping);
```

普通应用应优先使用类型化 Client API。`command()` 面向诊断工具；不要自行拼装未文档化
payload。它会在指定超时内等待匹配响应，协议/Agent 错误以 exception 报告。

<a id="sdk-parsers"></a>
## 13. 全部公开 parser

所有 parser 都是严格 parser：FrameType、payload 长度、版本或字段非法会抛异常。
先按类型分发，再调用对应函数：

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
    // 若需拥有 payload：prism::parseVideoChunk(frame)
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

通常不需要直接调用状态/配置 parser，因为对应 Client 方法已经完成 command 和解析。

<a id="sdk-system-upgrade"></a>
## 14. 系统升级

公开 SDK 只接受同时包含 Agent 和 sensor-board 固件的完整 ZIP，不提供单独刷写某一
组件的公共 API。

### 14.1 离线检查升级包

```cpp
const auto package =
    prism::inspectSystemUpgradePackage("prism-system-update.zip");
std::cout << package.package_version << ' '
          << package.agent_version << ' '
          << package.sensor_board_version << '\n';
```

检查不需要打开设备。

### 14.2 执行升级

```cpp
const std::string package_path = "prism-system-update.zip";
const auto package = prism::inspectSystemUpgradePackage(package_path);
std::cout << package.agent_version << ' '
          << package.sensor_board_version << '\n';
if (client.streamTransferActive()) {
  throw std::logic_error("先停止所有数据流");
}
// 仅在用户确认 package 中的两个版本且设备供电稳定后继续。
prism::UpgradeOptions options;
options.version.clear();  // public upgradeSystem() 的保留字段
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
if (!result.complete) throw std::runtime_error("系统升级未完成");
```

升级要求所有流停止。phase enum 包含 `ValidatingPackage`、`Agent`、`SensorBoard` 和
`Complete`；实际执行顺序是 `ValidatingPackage → SensorBoard → Agent → Complete`（enum 数值顺序不代表
传输顺序）。升级后 Agent 版本必须继续与 Host SDK 精确匹配，否则旧应用会按设计
拒绝连接。

`chunk_size` 允许 1..1048576 bytes；`wait_for_restart=true` 时
`restart_timeout_ms` 允许 5000..300000 ms。进度回调在调用线程同步执行，必须快速、
不抛异常，也不能重入同一 `Client`。

`parseUpgradeStatus()` 和 `parseSensorBoardUpgradeStatus()` 仅供自定义低级升级工具；
常规应用使用 `upgradeSystem()`。

<a id="sdk-thread-safety"></a>
## 15. 线程、安全和异常

- 同一时刻只允许一个进程占用设备 USB interface；
- 使用一个线程作为 `Client` I/O owner；
- 不要从多个线程并发调用 `readFrame()` 或 Client command；
- `ImuStream`/`LidarStream` 不创建线程，handler 同步执行；
- handler 抛出的异常会从 `handleFrame()` 继续向外传播；
- `VideoChunkView` 和 Stream callback 的引用需要跨线程时必须复制；
- JPEG 解码、点云渲染、文件写入放到有界 worker 队列；
- 时间同步、持久配置、LiDAR 网络、Wi‑Fi 和升级前确认
  `streamTransferActive()==false`；
- Stream 析构会尝试停止并吞掉停止异常，正常流程应显式 stop；
- USB 拔出、超时、版本不匹配、参数非法和设备端拒绝均通过 C++ exception 报告。

应用边界：

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

Client 断连后不要继续使用旧 Stream wrapper；销毁它们，重新枚举并建立新的 Client。

<a id="sdk-windows-runtime"></a>
## 16. Windows Runtime API v5

运行时入口：

```cpp
using GetApi = prism::GetRuntimeApiFunction;
auto entry = reinterpret_cast<GetApi>(
    GetProcAddress(module, prism::kRuntimeApiEntryPoint));
if (entry == nullptr) {
  throw std::runtime_error("Prism Runtime API 入口不存在");
}
const prism::RuntimeApi* api = entry(prism::kRuntimeApiVersion);
if (api == nullptr) {
  throw std::runtime_error("Prism Runtime API v5 不受支持");
}
```

调用前必须检查：

- `abi_version == 4`；
- `struct_size >= sizeof(prism::RuntimeApi)`；
- `sdk_version == "1.0.0"`；
- `api->msvc_version / 100 == _MSC_VER / 100`，即 DLL 和应用属于兼容的
  MSVC 14.x runtime family；
- 将要使用的函数指针非空。

Client 生命周期示例：

```cpp
prism::Client* client = api->client_create();
if (client == nullptr) {
  throw std::runtime_error("无法创建 Prism Client");
}
try {
  const auto devices = api->enumerate(prism::kDefaultVid,
                                      prism::kDefaultPid);
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

Runtime API 字段与直接 API 的对应关系：

| Runtime API | 等价功能 |
| --- | --- |
| `client_create/destroy` | Client 构造/析构 |
| `enumerate/open_device/close_device/is_open/path/serial_number` | 设备生命周期 |
| `keepalive_enabled/stream_transfer_active` | 状态查询 |
| `hello/device_info/device_versions` | 版本与健康 |
| `synchronize_system_time` | 设置并验证设备系统时间 |
| `network_info` | RK 网络信息 |
| `wifi_hotspot_status/set_wifi_hotspot_enabled` | Wi‑Fi AP |
| `device_configuration/save_device_configuration` | 持久配置 |
| `camera_exposure/set_exposure_configuration` | 运行时曝光 |
| `camera_exposure_limits/set_camera_exposure_limits` | 自动曝光和 gain 上下限 |
| `start_video/stop_video/start_imu/stop_imu/send_video_ack` | Camera/IMU |
| `start_lidar/stop_lidar/lidar_status` | LiDAR 控制 |
| `lidar_network_status/save_lidar_network_configuration/probe_lidar_network` | LiDAR 网络 |
| `read_frame` | 唯一接收循环 |
| `inspect_system_upgrade_package/upgrade_system` | 系统升级 |
| `parse_heartbeat` | Heartbeat |
| `parse_video_chunk_view/parse_video_meta/parse_imu_sample` | Camera/板载 IMU |
| `parse_lidar_point_batch/parse_lidar_imu_sample` | LiDAR 数据 |
| `usb_link_speed_name/sensor_board_error_code_name` | 显示辅助 |

Windows 没有 `ImuStream`/`LidarStream` 包装类，应使用 `read_frame + switch + parser`
实现第 8–11 节相同的分发逻辑。`VideoChunkView::data` 的生命周期规则完全相同。
必须先销毁所有 Client 和由 DLL 返回的 C++ 对象，最后才能 `FreeLibrary()`。

<a id="sdk-build-integration"></a>
## 17. CMake 集成与应用分发

仓库根目录可直接编译所有适用于当前平台的示例：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

将 `examples/` 复制到独立工程后，可通过 `PRISM_SDK_ROOT` 指定本二进制包：

```bash
cmake -S examples -B build-example \
  -DPRISM_SDK_ROOT=/absolute/path/to/Prism-SDK \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-example --config Release --parallel
```

Linux/macOS 应用链接对应 SDK 动态库；发布时保留示例 CMake 中的相对 RPATH。
macOS 还需随应用分发 `libusb-1.0.0.dylib`。Windows 只包含 DLL，应使用第 16 节的
Runtime API loader，并将 `prism_usb_sdk.dll` 放在 exe 旁边。完整平台依赖和布署要求见
[安装指南](installation.zh-CN.md)。
