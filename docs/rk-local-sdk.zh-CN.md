# RK-local SDK：C++ 接口

[English](rk-local-sdk.md) · [返回 SDK README](../README.zh-CN.md) · [验证记录](rk-local-sdk-testing.md)

供程序直接在 RK3576（Linux ARM64）运行，通过 `/run/prism/stream.sock` 访问
Agent。公开接口为 **C++17 `prism::rklocal::Client`**。与 Host SDK 的
`prism::Client` 共用 GNSS/RTK 返回类型和查询、改正流方法名，不经 USB，也不直接占用 UART。

## 从 C 迁移

这是公开接口的源码不兼容修改：不再发布 `prism/rklocal_sdk.h` 或 C 示例。
改用 `#include <prism/rklocal_sdk.hpp>`，更新头文件和 ARM64 静态库并重新编译；
不能将新头文件与旧静态库混用。底层 C 传输实现仅为私有实现细节，不再提供应用 C API。
SDK/Agent 握手仍为 1.1.0、协议 1；没有修改 Sensor Board 固件或通信协议，也不兼容旧 Agent。
USB 与 RK-local 并存需要 Agent 发送锁隔离修复，见[实机验证记录](rk-local-sdk-testing.md#physical-rk3576-validation)。
本工作树改动不等于已经更新 GitHub 的 v1.1.0 标签或 Release。

## 文件与编译

- [公共头文件](../include/prism/rklocal_sdk.hpp)
- [ARM64 静态库](../runtime/linux-arm64/libprism_rklocal_sdk.a)
- [CMake 导入目标](../cmake/PrismRkLocalSdk.cmake)：`Prism::RkLocal`
- [相机/IMU 示例](../examples/rklocal_capture.cpp)
- [只读 GNSS 示例](../examples/rklocal_gnss_status.cpp)
- [手动实机自测](../rk-local-sdk/tests/hardware_test.cpp)（不会由 CTest 自动采集）

在 RK 上从 SDK 仓库根目录运行：

```sh
cmake -S rk-local-sdk -B build/rklocal -DCMAKE_BUILD_TYPE=Release
cmake --build build/rklocal --parallel
ctest --test-dir build/rklocal --output-on-failure
# 10 秒、四相机 + IMU0；不写图像文件；运行时请求 30 FPS
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 - 1
# 可选：只保存第一组四张 JPEG，目录必须不存在
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 ./first-jpegs 1
# 100 次只读查询，目标 10 Hz；没有有效 GPS 不代表 SDK 调用失败
build/rklocal/prism-rklocal-gnss-status /run/prism/stream.sock 100
```

只安装 IMU0 时保持最后参数为 1；两颗都存在才使用 2。采集时 Ctrl-C 会正常停止。
交叉编译可追加 `-DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/aarch64-linux-gnu.cmake` 和
`-DPRISM_AARCH64_CROSS_PREFIX=/path/to/aarch64-none-linux-gnu-`。
交叉编译的测试需在 ARM64 设备或模拟器上执行，不能直接当 x86-64 程序运行。

接入自己的工程：

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_rk_app LANGUAGES CXX)
include("/opt/Prism-SDK/cmake/PrismRkLocalSdk.cmake")
add_executable(my_rk_app main.cpp)
target_link_libraries(my_rk_app PRIVATE Prism::RkLocal)
```

目标传递C++17、头文件、静态库、pthread和dl；需要系统C/C++运行库。
miniz和OpenSSL libcrypto已静态纳入，无需libusb或libcrypto.so/libssl.so。
源码构建需要目标架构OpenSSL开发头文件和静态libcrypto.a（Ubuntu: libssl-dev）。

## 与 Host SDK 的一致范围及明确差异

连接后的以下同名方法共用 Host 控制实现、参数/返回类型和默认参数。
编译测试逐项比较 51 个同名接口（含 RTCM 两个重载）的参数及返回类型；这不是 ABI 相同的承诺。

| 功能 | 两端同名接口 |
| --- | --- |
| 身份/版本/网络 | hello、deviceInfo、deviceVersions、boardTime、ping、networkInfo |
| 持久化配置 | deviceConfiguration、saveDeviceConfiguration |
| 运行时曝光 | cameraExposure、setExposureConfiguration、setCameraExposure、setAutoExposureTargetBrightness、cameraExposureLimits、setCameraExposureLimits |
| GNSS/PPS/RTK | gnssTimingStatus、rtkNavigationStatus、rtkCorrectionStatus、timeSyncPortStatus、setTimeSyncPortMode |
| CORS输入 | beginRtkCorrections、sendRtkCorrections、endRtkCorrections |
| 相机/IMU | startVideo1280x1024、startImu、stopVideo、stopImu、sendVideoAck |
| LiDAR | startLidar、stopLidar、lidarStatus、lidarNetworkStatus、saveLidarNetworkConfiguration、probeLidarNetwork |
| rover原始流 | startRoverRtcm、stopRoverRtcm |
| Wi-Fi热点 | wifiHotspotStatus、setWifiHotspotEnabled |
| 校时/升级 | synchronizeTimeNtpLike、synchronizeSystemTime、upgradeSystem |
| 底层/生命周期 | command、readFrame、streamTransferActive、setKeepaliveEnabled、keepaliveEnabled、path、serialNumber、isOpen、close、closeDevice |

以下部分**不一致或不能保证直接替换**：

- 连接/类型：Host 是 `prism::Client`，RK-local 是 `prism::rklocal::Client`。
  USB `enumerate/openFirst/openFirstDevice/open(DeviceInfo)` 不适用；
  本机使用 `open(ClientOptions)/openDevice(ClientOptions)`。
  `path()` 返回 Unix socket 路径，`serialNumber()` 为空，不伪造 USB 序列号。
- 时钟来源：同名 `synchronizeSystemTime()` 使用调用进程的系统时钟。
  本机进程与 Agent 共用 RK 时钟，不能由此获得外部 UTC。
  本机专有 `setDeviceTime(utc_us, timeout_ms=25000)` 接受入口时刻的外部 UTC 微秒，
  按单调时钟补偿经过时间；Agent 设置 Sensor Board，再由 PPS 驯化 RK 并验证 PHC。
  GPS 锁定或传输活动时拒绝写入；连接不会自动校时。
  该入口返回一次校正/验证，`before` 保持默认，并非 Host 多样本报告。
- 读取：本机额外提供 `startCapture/stopCapture/readFrameSet/readImu/readRtkNavigation`。
  内部线程组装相机/IMU并自动 ACK；默认 `readFrame()` 只排队其他原始事件。
  连接时设 `raw_camera_imu_frames=true` 才收到原始相机/IMU，无需再次 ACK。
  Host 的 VideoStream/ImuStream/LidarStream/RoverRtcmStream 接受 Host Client，
  不能直接传入 RK-local Client；改用 `readFrame()` + 公共 `parse*` 函数或本机已解包读取接口。
- 缓存：原始队列默认 256 帧 / 16 MiB，超限丢最旧非关键事件，检查 `droppedRawFrames()`。
  无法安全保存升级关键进度时报错。这不是无丢包记录器；复制原始图像增加内存/CPU开销。
- IMU默认值：同名 `startImu()` 均默认2颗；本机专有 `startCapture()` 默认1颗，支持仅IMU0。
  任一停止路径都同时停止 Camera 和 IMU。
- 超时：同名 `command/readFrame/sendRtkCorrections` 默认3000ms。
  RTCM指针/vector重载都支持末尾timeout_ms，每个最多16KiB的命令分别计时，而非整流总超时。
  高层命令复用Host专用超时。本机 `ClientOptions.command_timeout_ms` 默认10000，
  仅用于握手与专有聚合采集控制，不覆盖同名方法。
  超时不会撤销已发出的写入；先查询状态，不要盲目重试。
- 异常：共用控制/解析层使用标准 invalid_argument/logic_error/runtime_error；
  本机传输及专有读取可能抛 `prism::rklocal::Error`（继承runtime_error，带ErrorCode）。
  不保证具体异常子类与Host逐项相同；统一捕获 `std::exception`。
  本机close/isOpen增加noexcept；析构尽力清理不代表已确认停止成功。
- 二进制：两个类ABI不同，必须重新编译、使用成套头文件/库。
  当前不支持同一程序同时链接Host与本机静态库（共有codec符号），按部署位置选择一种SDK。
  本机发布产物仅Linux ARM64；socket权限和采集独占由Agent控制。
- 升级：共同入口仅接受联合ZIP（manifest.ini、prism-agent、BOOT.BIN）。
  本机重连原socket，Host重新枚举USB；升级到不同Agent版本后需要匹配SDK。
  实现已补齐；本轮仅模拟协议/失败路径验证，未验证真实刷写及重启回连。

IMU 首次 FSYNC 对齐前的时间戳可能未同步，进入 Sensor Board 时间域时可发生跳变。
请检查每个样本的 `TimestampSynced` 标志，不把未同步→已同步的跨域样本当作连续精确时间。
SDK 保留原始值，不丢弃或平滑时间戳。`sample_id` 用 uint32_t 承载 Sensor Board 的16位序号，
连续性需按65536回绕判断。

## 新增控制能力示例

以下写操作必须由应用明确触发，不是连接时自动执行：

```cpp
auto cfg = client.deviceConfiguration();
cfg.camera_fps = 30;
client.saveDeviceConfiguration(cfg, prism::kDeviceConfigFieldCameraFps); // 空闲、持久化
client.setAutoExposureTargetBrightness(45); // 运行时，不持久化
prism::CameraExposureConfiguration cam;
cam.mode = prism::CameraExposureMode::Manual;
cam.exposure_time_us = 1000;
cam.gain_x1024 = 2048; // 2x
client.setCameraExposure(0, cam);
auto hotspot = client.wifiHotspotStatus();
client.setWifiHotspotEnabled(true);
auto network = client.lidarNetworkStatus();
network.configuration.lidar_ip = "192.168.1.194";
client.saveLidarNetworkConfiguration(network.configuration);
client.probeLidarNetwork();
client.startLidar(prism::LidarModel::Mid360S);
auto frame = client.readFrame(3000); // 也可能先收到 heartbeat/RTK 事件
if (frame.type == prism::FrameType::LidarPoints) {
  auto points = prism::parseLidarPointBatch(frame);
}
client.stopLidar();
client.startRoverRtcm();
frame = client.readFrame(3000);
if (frame.type == prism::FrameType::RoverRtcm) {
  auto bytes = prism::parseRoverRtcmChunkView(frame); // view生命周期=frame生命周期
}
client.stopRoverRtcm();
// 实际刷写：须停止传输、电源稳定、确认更新包版本和哈希
client.upgradeSystem("/path/prism-system-update.zip", {},
    [](const prism::SystemUpgradeProgress& p) { /* 显示进度，不重入Client */ });
```

Wi-Fi只开放与Host相同的热点查询/启停，不支持任意SSID/密码写入。
TimeSync固定SensorBoardMaster；已废弃的PpsNmeaOutput同样拒绝。
SDK不登录CORS，输入应用剥离HTTP/ICY头后的原始RTCM2.x/RTCM3；
rover输出是接收机CRC校验通过的RTCM3，不是CORS回显。

## 连接与资源管理

```cpp
#include <prism/rklocal_sdk.hpp>
#include <cstdio>

int main() {
  try {
    auto client = prism::rklocal::Client::open();
    const prism::GnssTimingStatus gps = client.gnssTimingStatus();
    std::printf("satellites=%u, NMEA age=%u ms\n", gps.satellites, gps.nmea_age_ms);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  } // 自动关闭；连接不会自动设置时间
}
```

`ClientOptions` 默认：socket 如上、命令超时 10000 ms、IMU 队列 8192 条、
四相机帧组队列 4 组。SDK 自行维护接收与 1 Hz keepalive。
`Client` 可移动、不可复制；对已打开对象再次 open 会报 Busy。
`isOpen()` 只表示对象持有连接，不是 Agent 健康检查。
`close()`/析构为不抛异常的尽力清理；需要确认停止成功时显式调用 `stopCapture()`。

Agent 是 UART/CSI/V4L2 和采集的唯一所有者。只允许一个本地采集控制者。
同一个 Client 的生命周期、控制和查询操作由应用串行调用，关闭前先结束读取线程；
不要一边销毁/移动 Client，一边在其他线程使用它。

## 相机和 IMU

```cpp
prism::rklocal::CaptureConfiguration capture;
capture.camera_fps = 30;
capture.imu_rate_hz = 800;
capture.imu_sensor_count = 1;
client.startCapture(capture); // 相机+IMU整体启动，不存在相机-only模式
if (auto imu = client.readImu(1000)) {
  // imu->accel_mg / gyro_mdps / timestamp_us
}
if (auto frames = client.readFrameSet(1000)) {
  const auto& jpeg = frames->image[0];
  // jpeg.data.get(), jpeg.size：完整 JPEG；在 frames 的生命周期内使用
}
client.stopCapture();
```

采集配置默认频率 0（使用设备配置），IMU 数量 1。非零 FPS 目前支持整数 1..30，
IMU 频率支持 800 Hz；这里不修改持久化配置。
`FrameSet` 可移动不可复制，包含四个 `Image` 和曝光/增益/触发 metadata。
JPEG 缓冲区转移所有权，不额外复制图像字节；对象析构自动释放，且可以比 Client 活得更久。
异步处理使用 `std::move` 转交整个 FrameSet，不要保存悬空指针。
队列满时丢最旧数据，不反压 Agent；应用应监测 sample/frame ID 连续性。

| 数据 | 单位 / 含义 |
| --- | --- |
| IMU `accel_mg[3]` / `gyro_mdps[3]` | mg / 毫度每秒 |
| `temp_milli_c` | 毫摄氏度 |
| IMU/Image/FrameSet `timestamp_us` | Sensor Board 时间线，微秒 |
| metadata `trigger_time_ns` / `exposure_us[4]` | 纳秒 / 微秒 |
| metadata `analog_gain_x1024` / `digital_gain_x1024` | 实际增益 ×1024 |

## GPS、PPS 和 RTK 数据

`gnssTimingStatus()` 返回定位有效性、卫星数、DOP、经纬度、海拔、UTC、
`nmea_age_ms`、更新计数、外部锁定 `time_synced`、PPS 检测/有效性/脉宽。
`offset_fresh` 为真时，`message_pps_offset_us` 才表示对应 PPS 后第一条有效 RMC
的延迟（0..800000 µs）。不公开 RK-PPS 精度诊断，保留的 reserved 字段不可使用。

坐标使用前检查 `nmea_seen`、fix/position 有效位和 age；GGA 经纬度为度×1e7、
海拔/大地水准面分离为 mm、DOP ×1000、UTC 为当天毫秒。GNSS 示例将超过 2 秒的数据
视为过期，应用可按场景设置阈值。PPS valid 不等同于外部 UTC 授时成功。

`rtkNavigationStatus()` 查询当前结果；`readRtkNavigation()` 等待最新事件。
原始位置受 `solution_valid` 控制，独立平滑位置受 `smoothed_position_valid` 控制。
保留各自 epoch、FIX/FLOAT 状态、经纬度/椭球高、E/N/U 标准差、质量门控与重置计数。
经纬度单位度，高度/标准差单位米；米制 ENU 位置须用共同原点转换，不能把标准差当位置。
用设备 UTC 检查解的 age；读取成功不等于有新解，GNSS 10 Hz 也不保证每次都出 RTK 解。

## CORS / 原始 RTCM 输入

```cpp
// bytes 是应用从 NTRIP 去掉 HTTP/ICY 头后获得的原始二进制改正流。
client.beginRtkCorrections();
try {
  client.sendRtkCorrections(bytes); // std::vector<uint8_t>，也支持 data + size
} catch (...) {
  try { client.endRtkCorrections(); } catch (...) {}
  throw;
}
const auto status = client.endRtkCorrections();
```

应用负责 NTRIP 登录、GGA、重连；SDK 不保存账号密码、不把文本/Base64 当 RTCM。
可连续多次 send，同一会话不要混合格式；超过 16 KiB 会自动分块，空输入报错。
超时后先查询状态，不要盲目重发整块导致重复。Agent 自动识别 RTCM2.x 或 RTCM3.x；
`status.correction_format` 为 Unknown/Rtcm2/Rtcm3/Unsupported，并上报字节、报文、
观测历元、解算及解码错误计数。这些数据给 RK 解算器，不转发给 UM960/Sensor Board。
仅识别格式并不代表已达到 FLOAT/FIX；还须检查导航结果和新鲜度。

## 超时与错误

三个 read 方法返回 `std::optional<T>`：超时/无数据返回 `std::nullopt`；
timeout_ms=0 为非阻塞，`kWaitForever` 为一直等待至有数据或断开。
`readFrame()`超时抛异常而非optional。控制/解析层使用标准异常；
本机传输/读取错误可抛 `prism::rklocal::Error`。
`e.what()` 为诊断，`e.code()` 区分 InvalidArgument、System、Protocol、Timeout、
Closed、Busy、Remote、VersionMismatch。不要把错误当作“没有 GPS”或“没有新帧”。

旧 C 迁移速查：open→Client::open，start/stop→startCapture/stopCapture，
read_imu/read_frame_set→readImu/readFrameSet，get_gnss_status→gnssTimingStatus，
get_rtk_navigation→rtkNavigationStatus，last_error→异常，frame_set_release→自动析构。
