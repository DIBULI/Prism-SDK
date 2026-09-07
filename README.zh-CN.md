# Prism Host SDK 1.1.0

同时包含 **RK-local SDK 1.1.0**：设备本机 C++17 Client 接口、ARM64 静态库、相机/IMU
采集示例和 GNSS 状态查询示例。见 [RK-local 使用说明](docs/rk-local-sdk.zh-CN.md)
及 [1.1.0 GNSS/RTK 接口说明](docs/gnss-rtk.zh-CN.md)。

[![Build SDK Examples](https://github.com/DIBULI/Prism-SDK/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/DIBULI/Prism-SDK/actions/workflows/build.yml)

[English](README.md)

本仓库是 Prism Host SDK 的二进制发布仓库，只包含公共 C++ 头文件、三个受支持
平台的预编译动态库、Linux x86-64/arm64 静态库、用户安装/使用文档和 CMake 示例，
不包含 SDK 实现源码或设备固件源码。

## 仓库内容

```text
Prism-SDK/
├── include/prism/                 C++17 公共头文件
├── runtime/
│   ├── linux-x64/                 Ubuntu 20.04+ x86-64 .so 与 .a
│   ├── linux-arm64/               Ubuntu 20.04+ arm64 .so 与 .a
│   ├── ros/
│   │   ├── linux-x64/             所有受支持 ROS/Ubuntu x86-64 版本
│   │   └── linux-arm64/           所有受支持 ROS/Ubuntu ARM64 版本
│   ├── macos-arm64/               macOS 13+ Apple Silicon 动态库
│   └── windows-x64/               Windows 10/11 x64 DLL
├── rk-local-sdk/                 本机示例构建入口与接口测试
├── docs/                          Host/RK-local 接口与使用文档
├── examples/                      经编译验证的 SDK 示例
├── CMakeLists.txt
├── ORIGIN.md                      发布来源记录
└── SHA256SUMS                     文件完整性校验
```

## 兼容要求

- 分发版本：`1.1.0`
- Host SDK 运行时/ABI：`1.1.0`
- Runtime API：`12`
- USB protocol：`1`
- 设备 Agent：必须为 `1.1.0`
- C++：C++17 或更新版本
- CMake：3.20 或更新版本

1.1.0 分发包面向全部受支持平台打包 Host SDK 1.1.0 接口，并包含从同一
1.1.0 SDK 源码基线编译的 Linux ARM64 产物。运行时严格执行 SDK 1.1.0 与
Agent 1.1.0 的版本握手。不要混用不同版本的头文件和库，也不要连接非 1.1.0
的 Agent。

### 各 Release Tag 兼容关系

| SDK Release Tag | 分发版本 | Host SDK 运行时/ABI | 支持的 Agent | 已验证的 sensor-board | USB 协议 |
| --- | --- | --- | --- | --- | --- |
| `v1.1.0` | `1.1.0` | `1.1.0` | `1.1.0` | `0.4.26` | `1` |

Host SDK 会在打开设备时拒绝不兼容的 Agent。Agent 会上报 sensor-board 版本，
但 Host SDK 不会单独拒绝该版本，因此请使用表中对应 Release Tag 已验证的
sensor-board 版本。

GitHub Actions 会通过三平台矩阵编译每一个 example 源文件，运行全部无需设备的支持
测试，对发布的动态库执行加载冒烟测试，并验证两个 Linux 架构的静态链接。新增
`examples/*.cpp` 如果没有注册 CMake target，配置会直接失败，避免后续示例被 CI
静默漏编。

Linux x86-64 动态库采用 Ubuntu 20.04/GCC 9 ABI 基线并静态包含 OpenSSL，
因此同一份 `.so` 支持 Ubuntu 20.04、22.04、24.04 和 26.04；libusb 仍通过
稳定的 `libusb-1.0.so.0` SONAME 动态链接。`runtime/ros` 下分别提供一份覆盖
全部 ROS/Ubuntu 版本的 x86-64 动态前缀和 ARM64 静态前缀。普通桌面 SDK 用户
默认使用
`runtime/linux-x64/libprism_usb_sdk.so`。配置时设置
`PRISM_SDK_USE_STATIC=ON` 可改用同目录的 `libprism_usb_sdk.a`。

## Linux 静态 SDK

安装 OpenSSL 与 libusb 开发包，然后在配置使用方时启用静态 SDK：

```bash
sudo apt-get install -y libssl-dev libusb-1.0-0-dev
cmake -S . -B build-static \
  -DCMAKE_BUILD_TYPE=Release \
  -DPRISM_SDK_USE_STATIC=ON
cmake --build build-static --config Release
```

这样最终程序不再依赖 `libprism_usb_sdk.so`。OpenSSL 和 libusb 仍是传递链接依赖，
默认使用它们的动态库；只有使用方显式选择兼容的静态版本时才会进一步静态链接。

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

运行示例及手动校时的用法见[示例使用说明](docs/examples.zh-CN.md#prism-device-info-time-sync)。

## 文档

接口使用文档统一维护在 `docs/`，从[文档目录](docs/README.zh-CN.md)进入。

- [1.1.0 更新说明](docs/update/v1.1.0.zh-CN.md)
- [Release 1.1.0 update notes](docs/update/v1.1.0.md)
- [完整 SDK 开发手册](docs/development-guide.zh-CN.md)
- [逐接口 SDK 示例](docs/interface-examples.zh-CN.md)
- [Complete SDK development guide](docs/development-guide.md)
- [Per-interface SDK examples](docs/interface-examples.md)
- [安装指南](docs/installation.zh-CN.md)
- [SDK 使用指南](docs/usage.zh-CN.md)
- [Installation guide](docs/installation.md)
- [SDK usage guide](docs/usage.md)
- [示例说明](docs/examples.zh-CN.md)

RK-local 同名控制接口现已覆盖配置、曝光、LiDAR、热点、校时、升级和原始RTCM；
[一致范围与明确差异](docs/rk-local-sdk.zh-CN.md#与-host-sdk-的一致范围及明确差异)说明不能直接替换的部分。
