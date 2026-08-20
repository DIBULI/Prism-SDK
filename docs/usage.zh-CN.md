# SDK 使用指南

## Linux 与 macOS 基本流程

本节的 `prism::Client` 直连接口适用于 Linux 和 macOS。Windows 包不包含 import
library，Windows 应用必须按 `examples/device_info_time_sync.cpp` 使用 Runtime API v4。

统一包含：

```cpp
#include <prism/usb_sdk.hpp>
```

枚举、打开并读取设备状态：

```cpp
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("未发现 Prism 设备");
}

auto client = prism::Client::open(devices.front());
const auto hello = client.hello();
const auto versions = client.deviceVersions();
const auto info = client.deviceInfo();
```

连接多台设备时应按枚举结果中的 `DeviceInfo::serial_number` 选择，不要把临时 USB
path 当作稳定身份。`product_serial` 只有打开设备并调用 `client.deviceInfo()` 后才会
填充。

开始采集前至少检查：

- Camera 传输需要 `info.usb3_connected`；
- `info.sensor_board_online`；
- 需要同步时间戳时检查 `info.sensor_board_time_synced`；
- `info.camera_present_mask` 和 `info.imu_present_mask`；
- `info.imu_init_error_mask == 0`；
- `info.sensor_board_error_flags == 0`。

不要假定每台设备都安装两颗板载 IMU，应按检测到的数量和 mask 处理。

## 时间同步

Linux 和 macOS 可用 `synchronizeTimeNtpLike()` 测量设备相对主机的时间偏差，不修改
任何时钟：

```cpp
const auto measurement = client.synchronizeTimeNtpLike();
```

该测量接口同样要求 Camera、板载 IMU 和 LiDAR 数据流全部停止。Runtime API v4 没有
向 Windows 用户暴露这个只测量接口。

`synchronizeSystemTime()` 以主机墙钟为准校准设备，并回读验证：

```cpp
const auto result = client.synchronizeSystemTime();
```

正常返回表示已经通过验证；验证失败会抛出 exception。Windows 通过
`RuntimeApi::synchronize_system_time` 执行同一操作，具体见仓库示例。

设置设备时间前：

1. 确认主机 UTC 时间准确；
2. 停止 Camera、板载 IMU 和 LiDAR 数据流；
3. 调用 `synchronizeSystemTime()`；
4. 检查 `verified`、残余偏差和各时钟状态；
5. 操作结束后再重新开始采集。

该操作修改 RK `CLOCK_REALTIME`、Ethernet PTP 硬件时钟和 RK RTC，不会修改主机时钟。
它不会改变 sensor-board 的 GPS/NMEA 和 PPS 时间源，也不会自动让
`sensor_board_time_synced` 变为 true。录制过程中禁止跳变设备时间。

## 线程与独占访问

- 同一时间只能有一个进程占用设备 USB 接口；
- 使用单一线程作为 `Client` I/O owner；
- 不要从多个线程并发调用 `readFrame()`；
- JPEG 解码、点云渲染和写盘不能阻塞 USB 接收路径；
- 关闭 Client 或修改 idle-only 配置前先停止数据流。

## 错误处理

公共 API 通过 C++ exception 报告错误。应用边界应捕获 `std::exception` 并展示错误
信息。版本不一致、Linux USB 权限不足、设备已被其他程序占用、USB 断开，以及采集时
尝试设置时间，都属于需要明确提示的运行错误。

完整类型和接口说明以 `include/prism/` 下的公共头文件为准。
