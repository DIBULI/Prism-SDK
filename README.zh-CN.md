# Prism SDK 1.2.0

[English](README.md) · [接口文档](docs/README.zh-CN.md)

本仓库发布 Host SDK 和 RK-local C++17 SDK 的公共头文件、编译好的库、
消费者示例及文档，不包含 SDK 实现源码或设备固件源码。

## 版本与兼容性

- SDK 与设备 Agent 均为 **1.2.0**，连接时严格校验，不兼容其他 Agent 版本。
- 配套 Sensor Board **0.4.27**，USB/RK-local 协议版本均为 **1**。
- Windows Runtime API **18**，RTK-module 控制扩展版本 **1**。
- 头文件与库须同时更新，应用需要重新编译，不能混用旧库。

| 平台 | 发布文件 | 基线 |
| --- | --- | --- |
| Linux x64 / ARM64 | Host `.so`、`.a` | Ubuntu 20.04 ABI；CI 覆盖 20.04/22.04/24.04/26.04 |
| RK3576 Linux ARM64 | `libprism_rklocal_sdk.a` | RK-local C++ Client |
| macOS Apple Silicon | SDK 与 libusb `.dylib` | macOS 13 以上，不提供 Intel 包 |
| Windows x64 | SDK DLL，通过 Runtime API 加载 | MSVC 14.x、`/MD`，目标 Windows 10/11 |

Linux 动态库内嵌 OpenSSL，使用系统 libusb；Host 静态链接需要安装 libusb、
OpenSSL 开发包。RK-local 库内嵌其依赖，仅需 pthreads/dl 和系统 C/C++ 运行库。
`runtime/ros/` 是匹配的 SDK 安装前缀，不包含 ROS adapter 或 Docker 镜像。

## 本次更新

- Host/RK-local 统一 CORS 配置、RTK 启停接口。
- 最新 TimeSync 模式、RTK-module 版本、4G 与控制状态诊断。
- 接收机原生 GNSS/RTK 报文、天空图和轨迹模型；不提供 Agent 内部 RTK 解算及旧 raw/smoothed 接口。
- GNSS 输入诊断、XT32、雷达 line 字段、相机元数据。
- 详见[更新说明](docs/update/v1.2.0.zh-CN.md)及[产物来源](ORIGIN.md)。

## 编译示例

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Ubuntu/Debian 安装 `libusb-1.0-0-dev`；Host 静态链接还需 `libssl-dev`，
配置时添加 `-DPRISM_SDK_USE_STATIC=ON`。普通用户访问 USB 前安装权限规则：

```sh
sudo install -m 0644 runtime/ros/linux-x64/lib/udev/rules.d/99-prism-usb.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

ARM64 使用同一规则，必要时重新插拔 USB。Windows 使用 DLL Runtime API，
macOS 使用包内两个 dylib。RK-local 编译命令为
`cmake -S rk-local-sdk -B build/rklocal`，详见[接口说明与差异](docs/rk-local-sdk.zh-CN.md)。

连接客户端不会自动校时、采集或启动 CORS。保存账号与启动 RTK 是分开的操作；
启动前必须明确允许向已保存的 CORS 服务发送实时 GGA 位置。
Host 示例为 `prism-rtk-module-control --help`，顶层 ARM64 构建还提供
`prism-rklocal-rtk-module-control --help`。

所有接口说明集中在 [docs/](docs/README.zh-CN.md)。完整包验证：
`python3 scripts/test_all_examples.py --build-dir build-all-examples`。
