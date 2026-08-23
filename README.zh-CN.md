# Prism Host SDK 1.0.0

[![Build SDK Examples](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/xiangfuli/Prism-SDK/actions/workflows/build.yml)

[English](README.md)

本仓库是 Prism Host SDK 的二进制发布仓库，只包含公共 C++ 头文件、三个受支持
平台的预编译动态库、用户安装/使用文档和 CMake 示例，不包含 SDK 实现源码或设备
固件源码。

## 仓库内容

```text
Prism-SDK/
├── include/prism/                 C++17 公共头文件
├── runtime/
│   ├── linux-x64/                 默认 Ubuntu 22.04+ x86-64 动态库
│   ├── linux-arm64/               默认 Ubuntu 22.04+ ARM64 动态库
│   ├── ros/
│   │   ├── ubuntu-20.04-x86_64/   ROS 1 Noetic SDK 前缀
│   │   ├── ubuntu-22.04-x86_64/   ROS 2 Humble SDK 前缀
│   │   ├── ubuntu-24.04-x86_64/   ROS 2 Jazzy/Kilted SDK 前缀
│   │   └── ubuntu-26.04-x86_64/   ROS 2 Lyrical/Rolling SDK 前缀
│   ├── macos-arm64/               macOS 13+ Apple Silicon 动态库
│   └── windows-x64/               Windows 10/11 x64 DLL
├── docs/                          安装和使用文档
├── examples/                      经编译验证的 SDK 示例
├── CMakeLists.txt
├── ORIGIN.md                      发布来源记录
└── SHA256SUMS                     文件完整性校验
```

## 兼容要求

- Host SDK：`1.0.0`
- Runtime API：`5`
- USB protocol：`1`
- 设备 Agent：必须为 `1.0.0`
- C++：C++17 或更新版本
- CMake：3.20 或更新版本

SDK 会严格检查版本。不要混用不同版本的头文件、动态库或 Agent 固件。

GitHub Actions 会通过三平台矩阵编译每一个 example 源文件，运行全部无需设备的支持
测试，并对发布的动态库执行加载冒烟测试。新增 `examples/*.cpp` 如果没有注册 CMake
target，配置会直接失败，避免后续示例被 CI 静默漏编。

两个 Linux 架构使用相同的 Ubuntu 22.04 ABI 基线。仓库还在 `runtime/ros`
下提供 ROS Adapter 使用的完整多 Ubuntu ABI 二进制
SDK 前缀。Adapter 会按 ROS 发行版选择对应前缀；普通桌面 SDK 用户默认使用
`runtime/linux-x64/libprism_usb_sdk.so`。

## 编译示例

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建过程会把所需动态库复制到示例程序旁；macOS 还会复制配套的 libusb 动态库。

运行 GitHub Actions 同款的发布文件、全部编译目标和 CTest 自动化验证：

```bash
python3 scripts/test_all_examples.py --build-dir build-all-examples
```

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

- [完整 SDK 开发手册](docs/development-guide.zh-CN.md)
- [逐接口 SDK 示例](docs/interface-examples.zh-CN.md)
- [Complete SDK development guide](docs/development-guide.md)
- [Per-interface SDK examples](docs/interface-examples.md)
- [安装指南](docs/installation.zh-CN.md)
- [SDK 使用指南](docs/usage.zh-CN.md)
- [Installation guide](docs/installation.md)
- [SDK usage guide](docs/usage.md)
- [示例说明](examples/README.zh-CN.md)
