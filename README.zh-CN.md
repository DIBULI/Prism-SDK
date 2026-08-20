# Prism Host SDK 0.11.0

[English](README.md)

本仓库是 Prism Host SDK 的二进制发布仓库，只包含公共 C++ 头文件、三个受支持
平台的预编译动态库、用户安装/使用文档和 CMake 示例，不包含 SDK 实现源码或设备
固件源码。

## 仓库内容

```text
Prism-SDK/
├── include/prism/                 C++17 公共头文件
├── runtime/
│   ├── linux-x64/                 Ubuntu 24.04 x86-64 动态库
│   ├── macos-arm64/               macOS 13+ Apple Silicon 动态库
│   └── windows-x64/               Windows 10/11 x64 DLL
├── docs/                          安装和使用文档
├── examples/                      设备信息与时间同步示例
├── CMakeLists.txt
├── ORIGIN.md                      发布来源记录
└── SHA256SUMS                     文件完整性校验
```

## 兼容要求

- Host SDK：`0.11.0`
- Runtime API：`4`
- USB protocol：`10`
- 设备 Agent：必须为 `0.11.0`
- C++：C++17 或更新版本
- CMake：3.20 或更新版本

SDK 会严格检查版本。不要混用不同版本的头文件、动态库或 Agent 固件。

## 编译示例

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建过程会把所需动态库复制到示例程序旁；macOS 还会复制配套的 libusb 动态库。

只打开设备并读取设备信息：

```bash
./build/examples/prism-device-info-time-sync
```

读取设备信息并以主机时间校准设备：

```bash
./build/examples/prism-device-info-time-sync --sync-time
```

Visual Studio 等多配置生成器会把程序放在所选配置目录中，例如 `Release`。

时间同步会修改 RK `CLOCK_REALTIME`、Ethernet PTP 硬件时钟和 RK RTC。使用
`--sync-time` 前必须停止 Camera、IMU 和 LiDAR 数据流，并确认主机时间准确。该操作
不会替代 sensor-board 的 GPS/NMEA 与 PPS 同步源。

## 文档

- [安装指南](docs/installation.zh-CN.md)
- [SDK 使用指南](docs/usage.zh-CN.md)
- [Installation guide](docs/installation.md)
- [SDK usage guide](docs/usage.md)
- [示例说明](examples/README.md)
