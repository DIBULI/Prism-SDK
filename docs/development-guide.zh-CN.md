# Prism Agent SDK API 参考

本文档说明 Prism Agent SDK `1.2.0` 的公开 C++17 主机接口。SDK 通过 USB 与 Prism 设备端 agent 通信，在 Windows 上使用 WinUSB，在 Linux 和 macOS 上使用 libusb-1.0；三个平台使用相同的应用层 API。

统一头文件：

```cpp
#include "prism/usb_sdk.hpp"
```

> 当前线协议版本为 `1`，Agent、Host SDK 与 RK-local SDK 均为 `1.2.0`。Host SDK 与 agent 的语义版本必须完全一致。例如 Host SDK `1.2.0` 只接受 agent `1.2.0`。打开设备时 SDK 会自动执行严格的 `HELLO` 校验，版本不匹配时不会进入兼容模式。

## 运行要求

| 项目 | 要求 |
| --- | --- |
| 语言 | C++17 或更高 |
| Windows | WinUSB，链接由 CMake 目标自动处理 |
| Linux | libusb-1.0、OpenSSL Crypto、pthread，并安装仓库内 udev 规则 |
| macOS | libusb-1.0，SHA-256 使用系统 CommonCrypto；不需要 udev 规则 |
| 默认 VID | `0x2207` |
| 默认 PID | `0x1201` |
| Host SDK | `1.2.0` |
| 线协议 | `1` |

Linux 首次安装 udev 规则：

```sh
sudo install -m 0644 usb-sdk/udev/99-prism-usb.rules \
  /etc/udev/rules.d/99-prism-usb.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## 快速开始

```cpp
#include "prism/usb_sdk.hpp"

#include <iostream>

int main() {
  try {
    auto client = prism::Client::openFirst();

    const auto versions = client.deviceVersions();
    std::cout << versions.combined << "\n";

    const auto network = client.networkInfo();
    std::cout << network.ipv4 << "\n";

    client.closeDevice();
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
```

`Client` 为只移动类型，不支持复制。析构时会自动释放 USB 句柄，但应用仍应主动停止数据流并关闭设备。

## 设备发现与生命周期

### 发现设备

```cpp
static std::vector<DeviceInfo> Client::enumerate(
    uint16_t vid = kDefaultVid,
    uint16_t pid = kDefaultPid);
```

`enumerate()` 返回所有匹配设备：

| 字段 | 含义 |
| --- | --- |
| `path` | Windows 设备路径或 Linux/macOS libusb 总线/设备标识 |
| `serial_number` | USB 序列号，推荐作为 UI 中的设备选择标识 |
| `vendor_id` | 枚举使用的 VID |
| `product_id` | 枚举使用的 PID |

### 工厂式打开

```cpp
static Client Client::openFirst(
    uint16_t vid = kDefaultVid,
    uint16_t pid = kDefaultPid);

static Client Client::open(const DeviceInfo& device);
```

`openFirst()` 适合只连接一台设备的应用。多设备应用应先 `enumerate()`，再把用户选择的 `DeviceInfo` 传给 `open()`。

### 显式打开与关闭

```cpp
Client client;
client.openFirstDevice();
client.closeDevice();
```

完整生命周期接口：

```cpp
void openFirstDevice(uint16_t vid = kDefaultVid,
                     uint16_t pid = kDefaultPid);
void openDevice(const DeviceInfo& device);
void closeDevice();
void close();

bool isOpen() const;
std::wstring path() const;
std::wstring serialNumber() const;
```

同一个 `Client` 已打开时再次调用打开函数会失败。切换设备前必须先调用 `closeDevice()`。`close()` 与 `closeDevice()` 等价。

## DeviceInfo 设备状态快照

```cpp
DeviceInfo Client::deviceInfo();
```

该接口返回一次新的设备状态快照，空闲和相机/IMU 传输期间都可以调用。

| 字段 | 含义 |
| --- | --- |
| `usb_speed`、`usb3_connected` | 当前 USB 协商速率，以及是否为 SuperSpeed 或更高 |
| `detected_imu_count`、`imu_present_mask` | sensor-board 实际检测到的 IMU 数量和位掩码 |
| `imu_receiving_mask`、`imu_time_synced_mask`、`imu_init_error_mask` | 各 IMU 的接收、UTC 同步和初始化错误状态 |
| `imu_init_error_reason[0..1]` | 各 IMU 初始化失败的具体原因：WHO_AM_I、配置读回、FIFO 总线、样本超时或未知 |
| `detected_camera_count`、`camera_present_mask` | PL 中 SC130GS I2C 初始化成功的相机数量和位掩码；停止采集时仍有效，也可以报告部分安装 |
| `camera_streaming_mask` | RK carrier/ISP/JPEG 持续生成完整四路帧组时为 `0x0f`。四帧主机 credit 耗尽期间沿用最近一次健康状态，因为 USB 主机反压时无法继续观察生产端；不会报告部分传输 |
| `sensor_board_error_code`、`sensor_board_error_flags`、`sensor_board_error` | sensor-board/相机流水线当前故障分类、机器可读标志和可读原因；覆盖相机/TC 初始化、相机运行、DDR FIFO/AXI、TC 下溢/时钟丢失、相机未全部就绪、控制链路离线及四路帧流停滞。`0x01000000` 仅在有主机 credit 时，首帧超过 5 秒仍未产生，或后续完整帧组超过 4 秒未产生时置位；USB 主机反压不计为 sensor-board 故障 |
| `product_serial` | 产品/USB gadget 序列号 |
| `imu_fps`、`camera_fps` | 当前配置的 IMU 和 camera 帧率 |
| `sensor_board_online` | 板间控制链路是否在有效超时窗口内 |
| `sensor_board_time_synced` | sensor-board 当前是否具有有效 UTC 时间源 |
| `sensor_board_time_sync_source` | 底层来源元数据；界面用实时状态区分内部/外部授时，见[时间模型](time-sync-api.md) |
| `wifi` | WiFi 是否存在、启用策略、AP/DHCP 状态、接口、SSID、地址和错误 |

`enumerate()` 和状态接口共用 `DeviceInfo` 类型：枚举结果只填写 USB
传输身份；`client.deviceInfo()` 额外填写设备状态，并保留当前已打开设备的
路径、USB 序列号、VID 和 PID。

## 版本与基本信息

### Host SDK 版本

```cpp
std::string hostSdkVersion();
```

返回当前库内嵌的语义版本。

### HELLO

```cpp
HelloInfo hello();
```

重新执行严格握手并返回：

| 字段 | 含义 |
| --- | --- |
| `protocol_version` | 当前线协议版本 |
| `header_size` | 帧头长度 |
| `max_payload` | agent 接受的最大负载 |
| `app` | agent 应用标识 |
| `version` | agent 语义版本 |
| `process_id` | agent 进程 ID |
| `process_started_monotonic_us` | 用于判断 agent 是否重启的进程实例标识 |
| `update_result` | 上一次 agent 更新结果 |
| `update_version` | 上一次更新关联版本 |
| `sensor_board_version` | sensor-board 固件版本，尚未连接时为 `unknown` |

### 组合版本

```cpp
DeviceVersions deviceVersions();
```

这是独立的版本信息接口，与 `deviceInfo()` 和心跳分离，应用程序不需要从
设备状态字段推断固件版本。接口返回 `agent`、`sensor_board` 和便于显示的
`combined` 字符串：

```cpp
const auto versions = client.deviceVersions();
std::cout << versions.agent << "\n";
std::cout << versions.sensor_board << "\n";
std::cout << versions.combined << "\n";
```

### 时间、网络与连通性

```cpp
TimeInfo boardTime();
uint64_t ping();
NetworkInfo networkInfo();
```

`boardTime().unix_ms` 为设备系统 Unix 毫秒时间。

`NetworkInfo` 包含 `hostname`、`primary_interface`、`ipv4`、`netmask`、`gateway`、`mac`、`dns` 和摘要字符串。

## RTK-module CORS 配置与定位启停

Host `prism::Client` 和 RK-local `prism::rklocal::Client` 使用同一业务接口：

```cpp
auto cfg = client.timeSyncCorsConfiguration(); // 不返回密码
auto saved = client.saveTimeSyncCorsConfiguration(configuration);
auto running = client.startRtk(options); // 指定已确认配置代次，显式允许发送 GGA
auto stopped = client.stopRtk();         // 停止 RTK/CORS，保留授时
```

Agent 将账号保存在 RK，RTK-module 自行 NTRIP 登录、收流并送入接收机。
保存、已应用、已联网是三个不同状态；启停会确认同一命令代次的模块最终状态。
定位结果来自 `gnssObservations()` 的接收机原生报文；运行中不代表已 FIX。

详见[CORS 配置与 RTK 启停完整说明](rtk-module-control.md)、
[TimeSync 模式](timesync-port.md)和[模块版本查询](rtk-module-versions.md)。

## WiFi 热点

当前线协议提供以下空闲专用 API：

```cpp
prism::WifiHotspotStatus prism::Client::wifiHotspotStatus();
prism::WifiHotspotStatus prism::Client::setWifiHotspotEnabled(bool enabled);
```

两个接口都要求设备已经打开并通过版本校验。图像或 IMU 正在传输时，
SDK 会在发送命令前抛出 `std::logic_error`，因为命令响应与实时数据流
共享 USB 接收端点。`setWifiHotspotEnabled()` 会持久化请求的策略并返回
操作后的完整状态。

`WifiHotspotStatus` 字段：

| 字段 | 含义 |
| --- | --- |
| `version`、`size`、`flags` | 已严格校验的状态负载元数据 |
| `present` | 检测到 WiFi 硬件；AP 能力失败通过错误字段另行报告 |
| `enabled` | 已持久化的热点启用策略 |
| `running` | AP 与 DHCP 服务均在运行 |
| `ap_running` | AP 进程正在运行 |
| `dhcp_running` | DHCP 服务正在运行 |
| `persisted` | 请求的启用/禁用策略已保存 |
| `error_code`、`error` | 设备端启动错误（如有） |
| `interface_name` | 选中的 WiFi 接口 |
| `ssid` | 热点 SSID |
| `address` | 热点 IPv4 地址 |

没有已保存策略时默认启用热点。检测到的 WiFi 硬件支持 AP 模式时，SSID
为 RK CPU 序列号，热点为无需密码的开放网络；设备地址为
`10.42.200.1/24`，DHCP 由 `systemd-networkd` 提供。

没有 WiFi 设备不是硬错误：状态正常返回 `present=false`，agent 保留启用
策略，并监视之后接入的设备。热点启动失败也会返回可解析状态，
`error_code` 为非零且 `error` 提供说明，因此应用仍能展示其余状态字段；
这也包括检测到 WiFi 硬件但该硬件不支持 AP 模式的情况。

```cpp
const auto before = client.wifiHotspotStatus();
if (!before.running) {
  const auto after = client.setWifiHotspotEnabled(true);
  if (after.error_code != 0) {
    throw std::runtime_error(after.error);
  }
}
```

### WiFi 热点线协议消息

所有整数字段均为小端序。`WIFI_HOTSPOT_GET`（`0x0f`）请求负载为空。
`WIFI_HOTSPOT_SET`（`0x10`）请求负载固定为 8 字节：

| 偏移 | 类型 | 值 |
| ---: | --- | --- |
| 0 | `u16` | WiFi 负载版本，必须为 `1` |
| 2 | `u16` | 负载大小，必须为 `8` |
| 4 | `u32` | enabled，只能为 `0` 或 `1` |

两个请求都返回 `WIFI_HOTSPOT_STATUS`（`0x90`），负载固定为 208 字节：

| 偏移 | 大小 | 字段 |
| ---: | ---: | --- |
| 0 | 2 | WiFi 负载版本，必须为 `1` |
| 2 | 2 | 负载大小，必须为 `208` |
| 4 | 4 | 标志：设备存在、启用、AP 运行、DHCP 运行、已持久化 |
| 8 | 4 | 有符号错误码 |
| 12 | 16 | NUL 结尾的接口名 |
| 28 | 33 | NUL 结尾的 SSID |
| 61 | 16 | NUL 结尾的 IPv4 地址 |
| 77 | 128 | NUL 结尾的错误文本 |
| 205 | 3 | 必须为零的保留字节 |

SDK 会严格拒绝未知标志、未终止字符串、非零字符串填充或保留字节，以及
负载版本/大小不匹配。

## Keepalive 与离线保护

打开设备后 SDK 默认每秒发送一次 keepalive：

```cpp
void setKeepaliveEnabled(
    bool enabled,
    uint32_t interval_ms = kDefaultKeepaliveIntervalMs);
bool keepaliveEnabled() const;
```

若数据传输期间 agent 连续 5 秒没有收到 SDK keepalive，agent 会停止 IMU 和相机传输。生产应用应保持 keepalive 开启；关闭接口主要用于看门狗验证。

## 持久化配置

```cpp
DeviceConfiguration deviceConfiguration();

DeviceConfiguration saveDeviceConfiguration(
    const DeviceConfiguration& configuration,
    uint32_t field_mask = kDeviceConfigFieldAll);
```

当前支持的配置：

| 字段 | 支持值 |
| --- | --- |
| `camera_fps` | `1`、`2`、`5`、`10`、`15`、`20`、`25`、`30` |
| `imu_rate_hz` | `800`（ICM45686 固定输出频率） |
| `mjpeg_quality` | `1` 到 `99`，默认 `88` |
| `generation` | agent 维护的配置修订号 |
| `persisted` | 配置已持久化时为 `true` |

帧率和 MJPEG 质量修改必须在相机和 IMU 均停止时执行。保存后的 MJPEG
质量在下一次启动相机管线时生效。曝光不属于持久化配置。

字段掩码支持局部更新：

```cpp
auto config = client.deviceConfiguration();
config.imu_rate_hz = prism::kOnboardImuRateHz;
config = client.saveDeviceConfiguration(
    config, prism::kDeviceConfigFieldImuRateHz);
```

MJPEG 质量可独立修改：

```cpp
auto config = client.deviceConfiguration();
config.mjpeg_quality = 85;
config = client.saveDeviceConfiguration(
    config, prism::kDeviceConfigFieldMjpegQuality);
```

流启动函数中的速率参数为 `0` 时使用持久化配置；显式非零值只覆盖当前会话。

### TimeSync 接口模式

`timeSyncPortStatus()` 查询模式；`setTimeSyncPortMode(mode)` 显式设置并持久化。
支持 `GnssInput`（PPS/NMEA 输入）、`PpsNmeaOutput`（时间输出）、`Rtk`（RTK-module）。
切换前停止采集，输出模式必须先断开外部发送端。Agent 重启优先恢复手动保存模式。
模式已应用不等于接收机就绪或 CORS 已连接；选择 RTK 模式不会自动授权发送 GGA。
见[TimeSync 模式和安全操作](timesync-port.md)。

## 运行时相机曝光

曝光与增益控制不写入持久化配置，并允许在采集期间实时修改。四路 SC130GS
传感器增益可分别设置。自动模式完全由 PL 根据 RAW8 亮度闭环调整 TRIG0
曝光时间，同时保留配置的增益；传感器内部不再执行额外的自动曝光或自动
增益调节。

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

自动曝光的目标亮度是四路相机统一的 `1..255` 参数；手动曝光时间可按
camera 0..3 分别设置，上限为 `floor(1000000 / camera_fps) - 5000` 微秒。
1 到 30 之间任意整数 fps 的上限从 1 fps 的 995000 微秒到 30 fps 的
28333 微秒。`gain_x1024` 也可按相机
分别设置，范围为 `1024..126976`（`1x..124x`），步进 `32`（`1/32x`），
默认 `1024`（`1.0x`）。所有 setter 都返回 agent 完整回读结果，agent 或
sensor-board 重启后恢复设备默认值。

```cpp
client.setAutoExposureTargetBrightness(144);

prism::CameraExposureConfiguration exposure;
exposure.mode = prism::CameraExposureMode::Manual;
exposure.exposure_time_us = 250;
exposure.gain_x1024 = 3072;  // 3.0x
client.setCameraExposure(2, exposure);
```

当前曝光线协议为版本 2 和严格的 44 字节固定 payload，包括四路曝光时间与
四路 `gain_x1024`。兼容输入允许 28 字节 v1 设置请求并保留当前增益；所有
响应均为 v2。未知掩码、保留位或越界参数都会被拒绝。完整布局见
[运行时相机曝光](runtime-exposure.md)。

## NTP-like 时间同步

### 只测量偏差

```cpp
NtpTimeSyncResult synchronizeTimeNtpLike(
    uint32_t sample_count = 12,
    uint32_t timeout_ms = 1000);
```

该接口执行多次四时间戳交换，筛选低往返延迟样本，然后报告设备时间减主机时间的偏差：

| 字段 | 含义 |
| --- | --- |
| `offset_us` | 设备时间减主机时间 |
| `round_trip_us` | 最佳接受样本的往返时间 |
| `jitter_us` | 接受样本偏差的中位绝对偏差 |
| `device_time_us` | 方法返回时估算的设备时间 |
| `samples` | 每次四时间戳测量明细 |

该接口只测量，不修改任何时钟。

### 实际校准系统时间

```cpp
SystemTimeSyncResult synchronizeSystemTime(
    uint32_t sample_count = 12,
    uint32_t verification_sample_count = 6,
    uint32_t timeout_ms = 1000);
```

该接口以主机系统时间为 UTC 输入，但不会让 Agent 直接设置 RK 系统时钟。
Host 将未来整秒 UTC 和“完整命令接收到该秒”的延时交给 Sensor Board；Sensor Board
生成 PPS/UTC 给 RK，chrony 再同步 `CLOCK_REALTIME`，phc2sys 对齐以太网 PHC，最后
更新 RTC 并再次测量残差。外部 GNSS 已锁定时，Sensor Board 拒绝 Host fallback。
结果中的 `verified` 表示整条链路最终复测通过。

时间同步接口只能在非传输阶段调用：

```cpp
client.stopVideo();
client.stopImu();
client.stopLidar();

const auto measured = client.synchronizeTimeNtpLike();
const auto applied = client.synchronizeSystemTime();
```

```cpp
bool streamTransferActive() const noexcept;
```

可在 UI 启用时间同步按钮前检查 video、IMU 或 LiDAR 是否仍有数据流；SDK 与
Agent 都会拒绝在任一数据流活动时执行 `TIME_SYNC` 或 `TIME_SET`。IMU 的同步标志
只由 Sensor Board 的实际 PPS/UTC 状态产生；Host 不会直接伪造该标志。

## 系统心跳

agent 每秒发送一个 `Heartbeat` 帧：

```cpp
auto frame = client.readFrame(3000);
if (frame.type == prism::FrameType::Heartbeat) {
  const auto status = prism::parseHeartbeat(frame);
}
```

`HeartbeatStatus` 只有 `rk_system_time_us` 一个字段，即 RK 当前
`CLOCK_REALTIME` 的 Unix UTC 微秒值。heartbeat 不再携带 sensor-board、
IMU、camera、WiFi、FPS、USB 或序列号状态；这些状态统一通过
`Client::deviceInfo()` 获取。

## 图像传输

### 启动与停止

```cpp
VideoStatus startVideo1280x1024(uint32_t fps = 0);

void stopVideo();
```

当前模式为四路相机，每路 `1280x1024`，通过 USB 输出 MJPEG。

`fps=0` 使用持久化配置；显式值只允许 `10`、`20` 或 `30`。

`VideoStatus` 返回启用状态、相机数、帧率、宽高和负载大小。

### 接收与重组

```cpp
while (running) {
  const auto frame = client.readFrame(3000);

  if (frame.type == prism::FrameType::VideoChunk) {
    const auto chunk = prism::parseVideoChunkView(frame);
    // 使用 (chunk.camera_id, chunk.frame_id) 作为重组键。
  } else if (frame.type == prism::FrameType::VideoMeta) {
    const auto meta = prism::parseVideoMeta(frame);
    // 按 frame id 匹配元数据，不要按到达顺序匹配。
  }
}
```

`VideoChunk` 提供相机编号、图像尺寸、帧号、完整 JPEG 大小、分块偏移、分块长度、时间戳和实际分块数据。

高帧率接收路径应使用 `parseVideoChunkView()`，避免为每个 JPEG 分块再复制一次。
返回的 `VideoChunkView::data` 直接指向源 `Frame` 的载荷，只能在该 `Frame`
仍然有效且未被修改时使用。需要长期持有数据时继续使用会复制载荷的
`parseVideoChunk()`。

`VideoMeta` 提供触发时间、四路实际曝光/增益元数据和元数据 CRC。

`VIDEO_META` 载荷固定为 84 字节。前 24 字节的字段布局保持不变，随后依次为：
`trigger_time_ns`（字节 24–31）是 sensor-board 在四路公共 TRIG0 上升沿锁存的
Unix UTC 纳秒值；PPS/RMC 尚未同步时为 0。它不是曝光中心、MIPI 到达、ISP 输出
或 USB 交付时间。其后为 `exposure_us[4]`（字节 32–47）、
`analog_gain_x1024[4]`（字节 48–63）、`digital_gain_x1024[4]`
（字节 64–79）和 `meta_crc32`（字节 80–83）。
`exposure_us[4]` 始终表示对应图像帧真正采用的曝光时间；手动模式和 PL 自动
模式下有效范围都是 `50..(floor(1000000 / camera_fps) - 5000)` 微秒。自动模式报告 PL 亮度闭环为该帧选出的曝光时间，不是
默认值或请求值。`analog_gain_x1024[4]` 报告该帧实际采用、可由运行时曝光
接口设置的 SC130GS 模拟增益；`digital_gain_x1024[4]` 仍为只读实际元数据。
`meta_crc32` 直接来自 carrier 元数据头：采用带 `0xffffffff` 初值/终值异或的
反射式 IEEE CRC32；先将字节 84–87 的 CRC 字段置零，再计算完整 88 字节
carrier 头。

完整接收并接受一帧后应发送确认：

```cpp
void sendVideoAck(uint32_t last_frame_id);
```

## IMU 传输

### Client 级接口

```cpp
ImuStreamStatus startImu(
    uint32_t sensor_count = 2,
    uint32_t nominal_rate_hz = 0);

ImuStreamStatus stopImu();
```

`nominal_rate_hz=0` 使用持久化配置；显式值仅支持 `800` Hz。`stopImu()` 返回停止后的最终流状态。

### 推荐的 ImuStream 接口

```cpp
prism::ImuStream imu(client, [](const prism::ImuSample& sample) {
  // 处理结构化 IMU 数据。
});

imu.start();

while (running) {
  const auto frame = client.readFrame(3000);
  if (imu.handleFrame(frame)) {
    continue;
  }
  // 继续处理图像、元数据和心跳。
}

imu.stop();
```

`ImuStream` 不拥有 USB 接收线程；应用仍需保持唯一的 `readFrame()` 循环，并把每个帧交给 `handleFrame()`。

`ImuSample` 的主要字段：

| 字段 | 单位与含义 |
| --- | --- |
| `sensor_id` | IMU 编号 |
| `sample_id` | 每个 IMU 独立的单调样本计数 |
| `timestamp_us` | bit 7 置位时为 Unix UTC，否则为 sensor-board 本地时间 |
| `accel_mg[3]` | milli-g |
| `gyro_mdps[3]` | milli-degree/s |
| `temp_milli_c` | milli-degree Celsius |
| `fsync_event` | 当前样本为 FSYNC 后第一个 ODR 样本 |
| `fsync_delay_valid` | ICM-42688 FSYNC delay 字段有效 |
| `sample_gap` | 原始 IMU 时间戳检测到超过 4 个 ODR 的间隔 |
| `timestamp_synced` | `timestamp_us` 已同步为 UTC |

不要直接比较 IMU0 与 IMU1 的 `sample_id`，它们是两个独立计数器。跨 IMU 对齐应使用同步后的 `timestamp_us` 和 FSYNC 标志。

## 系统升级

公开 SDK 只接受包含 agent 和 sensor-board 固件的完整 ZIP，不提供单独升级某一组件的 API。

```cpp
SystemUpgradePackageInfo inspectSystemUpgradePackage(
    const std::string& package_path);

SystemUpgradeResult upgradeSystem(
    const std::string& package_path,
    const UpgradeOptions& options = {},
    const std::function<void(const SystemUpgradeProgress&)>& progress = {});
```

`inspectSystemUpgradePackage()` 不需要打开设备，用于验证 ZIP、清单、文件长度、SHA-256 和 agent 内嵌版本。

`upgradeSystem()` 要求设备已打开且处于空闲状态。SDK 会先传输并校验 sensor-board 固件，再提交 agent。若第一阶段失败，agent 不会被替换。

```cpp
const auto package =
    prism::inspectSystemUpgradePackage("prism-system-update.zip");

const auto result = client.upgradeSystem(
    "prism-system-update.zip",
    {},
    [](const prism::SystemUpgradeProgress& progress) {
      std::cout << progress.completed_bytes
                << "/" << progress.total_bytes << "\n";
    });
```

升级 ZIP 必须包含：

```text
manifest.ini
prism-agent
BOOT.BIN
```

若升级包内 agent 版本与当前 Host SDK 不同，升级后的 agent 会按设计拒绝旧 SDK。此时应关闭应用，并使用与新 agent 完全同版本的 Host SDK 重新连接。

## 低级帧接口

大多数应用应使用类型化接口。协议诊断或自定义工具可以使用：

```cpp
Frame readFrame(uint32_t timeout_ms = 3000);

Frame command(
    FrameType type,
    const std::vector<uint8_t>& payload = {},
    uint32_t timeout_ms = 3000);
```

`command()` 会等待对应命令响应，并只保留期间收到的最新一帧 heartbeat
供后续 `readFrame()` 读取；agent 返回错误帧时会抛出异常。

结构化解析函数：

```cpp
HeartbeatStatus parseHeartbeat(const Frame& frame);
DeviceInfo parseDeviceInfo(const Frame& frame);
VideoChunkView parseVideoChunkView(const Frame& frame);
VideoChunk parseVideoChunk(const Frame& frame);
VideoMeta parseVideoMeta(const Frame& frame);
ImuSample parseImuSample(const Frame& frame);
```

## 线程与接收循环

`Client` 本身不是线程安全对象。推荐由一个 I/O 线程拥有 `Client`，或由应用对所有调用加互斥锁。

图像、IMU、心跳和命令响应共享 USB IN 端点。应用必须只有一个读取者，按 `FrameType` 分发：

```cpp
while (running) {
  const auto frame = client.readFrame(3000);

  if (imu.handleFrame(frame)) {
    continue;
  }

  switch (frame.type) {
    case prism::FrameType::Heartbeat:
      onHeartbeat(prism::parseHeartbeat(frame));
      break;
    case prism::FrameType::VideoChunk:
      onVideoChunk(prism::parseVideoChunk(frame));
      break;
    case prism::FrameType::VideoMeta:
      onVideoMeta(prism::parseVideoMeta(frame));
      break;
    default:
      break;
  }
}
```

## 异常处理

协议、USB、超时和参数错误通常通过 `std::runtime_error` 报告；传输期间调用空闲专用接口会抛出 `std::logic_error`。

常见错误包括：

- 没有找到设备或 Windows 未绑定 WinUSB。
- USB 断开、设备重启或读写超时。
- Host SDK 与 agent 版本不匹配。
- 响应类型、负载长度或 CRC 不正确。
- 在数据传输期间请求 WiFi 热点状态/变更、时间同步、帧率修改或系统升级。

退出和异常恢复时应按顺序停止数据流：

```cpp
try {
  auto client = prism::Client::openFirst();
  client.startVideo1280x1024();
  client.startImu();

  // 接收循环。

  client.stopImu();
  client.stopVideo();
  client.closeDevice();
} catch (const std::exception& error) {
  // 记录 error.what()，释放 Client，再按应用策略重连。
}
```

## 推荐集成顺序

1. 使用 `enumerate()` 展示序列号，并打开用户选择的设备。
2. 依赖打开阶段的严格版本握手，不实现降级协议。
3. 使用 `deviceVersions()` 展示 agent 与 sensor-board 组合版本。
4. 在非传输阶段读取或变更 WiFi 热点状态，以及读取或保存持久化配置。
5. 用户点击采集后再启动图像和 IMU。
6. 保持一个 USB 接收循环，并按类型分发所有帧。
7. 图像按 `(camera_id, frame_id)` 重组，元数据按帧号匹配。
8. IMU 按 `(sensor_id, sample_id)` 检查连续性，跨 IMU 使用同步时间戳。
9. WiFi 热点操作、时间同步和系统升级前停止所有数据流。
10. 退出前停止 IMU、停止图像并关闭设备。
