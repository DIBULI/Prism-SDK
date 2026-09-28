# 快速开始：安装、连接与首次读取

[文档目录](../README.zh-CN.md) · [English](README.md)

<!-- page-toc -->
- [安装与链接](#安装与链接)
  - [Linux](#linux-x86-64-与-arm64) · [macOS](#macos-arm64) · [Windows](#windows-x64)
- [首次连接与读取](#首次连接与读取)
  - [时间同步](#时间同步)
<!-- /page-toc -->

<a id="installation"></a>

## 安装与链接

### 支持的平台

| 主机平台 | 架构 | 最低环境 | 库文件 |
| --- | --- | --- | --- |
| Linux | x86-64 | Ubuntu 20.04 或更新版本 | `runtime/linux-x64/libprism_usb_sdk.so` / `.a` |
| Linux | arm64 | Ubuntu 20.04 或更新版本 | `runtime/linux-arm64/libprism_usb_sdk.so` / `.a` |
| macOS | arm64 | macOS 13.0 | `runtime/macos-arm64/libprism_usb_sdk.dylib` |
| Windows | x86-64 | Windows 10/11 | `runtime/windows-x64/prism_usb_sdk.dll` |

两个 Linux 动态库均采用 Ubuntu 20.04/GCC 9 构建，静态包含 OpenSSL，动态依赖
libusb 及系统 C/C++ 运行库。来源见 ORIGIN.md。RK-local 静态库内含miniz/libcrypto，依赖pthread/dl和系统C++运行库，
不需要libusb或动态OpenSSL。

### Linux x86-64 与 arm64

安装构建工具和运行依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libusb-1.0-0
```

Ubuntu 20.04 自带 CMake 3.16，而示例需要 3.20 以上。可安装 pip 后执行
`python3 -m pip install --user -i https://pypi.tuna.tsinghua.edu.cn/simple cmake==3.31.6`，
并将 `$HOME/.local/bin` 加入 PATH；CI 在 20.04 使用 CMake 3.31.6 验证。

两个架构的 Host 动态库均不需要额外安装 OpenSSL 动态库。

安装 udev 规则，使普通用户能够打开 VID:PID `2207:1201`：

```bash
sudo tee /etc/udev/rules.d/99-prism-usb.rules >/dev/null <<'RULE'
SUBSYSTEM=="usb", ATTR{idVendor}=="2207", ATTR{idProduct}=="1201", MODE="0660", GROUP="plugdev"
RULE
sudo udevadm control --reload-rules
sudo udevadm trigger
```

必要时把用户加入对应用户组，重新登录并插拔 USB。Prism USB 接口同一时间只能由一个
应用占用。

开发时可以把 `libprism_usb_sdk.so` 放在程序旁并配置 `$ORIGIN` RPATH，也可以放进
系统动态库搜索路径。本仓库示例采用程序私有动态库方式。

如果要静态链接 Prism SDK 实现，安装 `libusb-1.0-0-dev` 和 `libssl-dev`，并在配置
仓库示例或使用方时执行：

```bash
cmake -S . -B build-static -DPRISM_SDK_USE_STATIC=ON
cmake --build build-static --config Release
```

生成的程序不再依赖 `libprism_usb_sdk.so`。除非使用方另外选择兼容的静态版本，
OpenSSL 和 libusb 默认仍采用动态链接。

### macOS arm64

编译示例前安装 Xcode Command Line Tools 和 CMake：

```bash
xcode-select --install
brew install cmake
```

这里 Homebrew 只用于安装 CMake；发布的 SDK 动态库不含 Homebrew 绝对加载路径。

必须同时保留：

```text
libprism_usb_sdk.dylib
libusb-1.0.0.dylib
```

两个库都使用可迁移的 `@rpath`。打包 `.app` 时，将它们复制到
`Contents/Frameworks`，给主程序加入 `@executable_path/../Frameworks` RPATH，最后
统一签名。分发配套 libusb 时要保留 `libusb-COPYING.txt`。

如果系统可以枚举设备但打开被拒绝，先关闭已占用 Prism USB 接口的 Viewer 或命令行
程序，然后重新插拔设备。

### Windows x64

使用 Visual Studio 2022 C++ x64 工具链和 CMake。公共 C++ ABI 要求 MSVC 14.x 和
完全匹配的 SDK 1.2.0 头文件，不支持 MinGW。部署时安装最新版 Microsoft Visual C++
2015-2022 x64 Redistributable，并确保 Prism USB 接口使用 Windows WinUSB 驱动。将
`prism_usb_sdk.dll` 放在应用程序旁。

本仓库只发布 DLL，不发布 import library。Windows 应用应通过 `LoadLibraryW` 加载
DLL，解析 `prism_usb_sdk_get_runtime_api`，并在使用前验证 Runtime API 版本 18。仓库
示例已经实现该流程，并固定使用与 DLL 兼容的 `/MD` runtime 和 release iterator ABI，
即使用户选择 Debug 配置也是如此。

### 完整性检查

在仓库根目录执行：

```bash
sha256sum --check SHA256SUMS
```

macOS 使用 `shasum -a 256 -c SHA256SUMS`。

<a id="first-connection"></a>

## 首次连接与读取

本页是快速入门流程。全部公开控制、数据流、parser、升级和 Windows Runtime API
接口的说明与示例见 [完整 SDK 开发手册](../reference/host.zh-CN.md)。

### Linux 与 macOS 基本流程

本节的 `prism::Client` 直连接口适用于 Linux 和 macOS。Windows 包不包含 import
library，Windows 应用必须按 `examples/device_info_time_sync.cpp` 使用 Runtime API v18。

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

### 时间同步

Linux 和 macOS 可用 `synchronizeTimeNtpLike()` 测量设备相对主机的时间偏差，不修改
任何时钟：

```cpp
const auto measurement = client.synchronizeTimeNtpLike();
```

该测量接口同样要求 Camera、板载 IMU 和 LiDAR 数据流全部停止。Runtime API v18 没有
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

Sensor Board 始终是主时钟。主机 UTC 经 Agent 送到 Sensor Board，再由其 PPS/NMEA
同步 RK，RK 向以太网提供 PTP 时间。外部 GNSS 已同步时 Agent 拒绝主机校时。
不依赖 RTC，也不修改主机时钟。录制过程中禁止跳变设备时间。

### 线程与独占访问

- 同一时间只能有一个进程占用设备 USB 接口；
- 使用单一线程作为 `Client` I/O owner；
- 不要从多个线程并发调用 `readFrame()`；
- JPEG 解码、点云渲染和写盘不能阻塞 USB 接收路径；
- 关闭 Client 或修改 idle-only 配置前先停止数据流。

### 错误处理

公共 API 通过 C++ exception 报告错误。应用边界应捕获 `std::exception` 并展示错误
信息。版本不一致、Linux USB 权限不足、设备已被其他程序占用、USB 断开，以及采集时
尝试设置时间，都属于需要明确提示的运行错误。

完整开发手册见上述链接；权威接口声明以 `include/prism/` 下的公共头文件为准。
