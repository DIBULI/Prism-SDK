# Prism SDK 示例

[English](README.md)

这些示例刻意保持小巧且默认安全：不会写入设备持久配置，不会修改曝光，不会升级固件，
也不会修改 Wi-Fi 或 LiDAR 网络配置。设备时间同步同样必须由用户显式启用。

## 编译

从仓库根目录编译全部示例：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

也可以把 `examples` 当作独立 CMake 工程编译：

```bash
cmake -S examples -B build-example -DCMAKE_BUILD_TYPE=Release
cmake --build build-example --config Release
```

同一时间只能有一个进程占用 Prism USB 设备。运行示例前请关闭 Prism Viewer 和其他
SDK 程序。

## `prism-device-info-time-sync`

平台：Linux x86-64、macOS arm64 和 Windows x64。

这个跨平台示例演示受支持的设备生命周期：

1. 枚举 Prism USB 设备；
2. 打开第一台设备；
3. 读取版本与设备健康状态；
4. 按需把设备时间同步到主机；
5. 正常关闭设备。

只读取设备信息：

```bash
./build/examples/prism-device-info-time-sync
```

设置并验证设备时间：

```bash
./build/examples/prism-device-info-time-sync --sync-time
```

`--sync-time` 是管理操作。主机时钟必须正确，而且 Camera、IMU 和 LiDAR 流必须全部
停止。在 Windows 上，本示例还演示如何加载 `prism_usb_sdk.dll`，验证 Runtime API
v5 后再调用接口。

## `prism-camera-imu-capture`

平台：Linux x86-64 和 macOS arm64。

这个限时示例启动 Camera/板载 IMU 聚合采集会话，在一个 USB 接收循环中通过
`ImuStream` 分发 IMU 样本，并解析 Camera 分块和元数据，最后显式停止两路采集。
示例严格要求 JPEG 分块 offset 连续，并在全部 Camera 图像和对应元数据到达后调用
`sendVideoAck()`。损坏或不完整的帧也会被退休并 ACK，防止耗尽设备流控 credit；但
无效元数据不会用于时间戳或曝光。示例不会保存 JPEG 数据。

使用设备持久配置中的 Camera FPS 和 IMU 频率采集 10 秒：

```bash
./build/examples/prism-camera-imu-capture
```

临时使用 20 FPS 和 500 Hz 采集 30 秒：

```bash
./build/examples/prism-camera-imu-capture \
  --seconds 30 --fps 20 --imu-rate 500
```

`--fps 0` 和 `--imu-rate 0` 表示使用设备持久配置。Camera FPS 可为 1 至 30 的任意
整数，IMU 频率可为 500 或 1000 Hz；这些启动参数不会写入持久配置。

示例仅在源 `Frame` 仍然存活时使用 `parseVideoChunkView()`。如果需要跨线程传递或在
下一次接收循环中继续使用 JPEG，必须先复制数据。

无需设备即可运行确定性的帧跟踪自测：

```bash
./build/examples/prism-camera-imu-capture --self-test
```

自测覆盖有效与无效元数据、连续多分块图像、重复分块、字节缺口，以及新帧号到达时
退休旧的不完整帧。CTest 中的测试名称为
`prism-camera-frame-tracker-self-test`。

## `prism-lidar-capture`

平台：Linux x86-64 和 macOS arm64。

这个限时示例在同一个 USB 接收循环中使用 `LidarStream` 获取点云批次和 LiDAR IMU
样本。必须显式指定雷达型号，因为 Mid360 与 Mid360S 使用不同配置，SDK 不会猜测
型号。

采集 Mid360 10 秒：

```bash
./build/examples/prism-lidar-capture --model mid360
```

采集 Mid360S 30 秒：

```bash
./build/examples/prism-lidar-capture \
  --model mid360s --seconds 30
```

运行本示例前，应在所有流停止时完成 LiDAR 网络配置与探测。示例会单独报告时间戳已
同步的样本数量；要求 UTC 的程序必须丢弃 `timestamp_synced=false` 的样本。

## 经编译检查的 API 示例目录

仓库还会编译按功能分组的源码目录，因此每个公开接口都会经过真实编译器检查，而不是
只存在于 Markdown 代码片段中：

| 可执行程序 | 源文件 | 平台 | 覆盖范围 |
| --- | --- | --- | --- |
| `prism-client-api-examples` | `client_api_examples.cpp` | Linux、macOS | Client 生命周期、枚举、设备状态、时间和 Wi-Fi |
| `prism-configuration-api-examples` | `configuration_api_examples.cpp` | Linux、macOS | 配置、曝光、采集、LiDAR 网络、底层命令和升级 |
| `prism-stream-api-examples` | `stream_api_examples.cpp` | Linux、macOS | `ImuStream` 与两个 `LidarStream` 构造方式及生命周期 |
| `prism-parser-api-examples` | `parser_api_examples.cpp` | Linux、macOS | 全部公共 helper 与帧解析器 |
| `prism-windows-runtime-api-examples` | `windows_runtime_api_examples.cpp` | Windows | Runtime API v5 的全部 45 个函数指针 |

### `prism-client-api-examples`

该源码中的每个函数分别提供 Client 构造与 move、枚举和打开、显式关闭、keepalive、设备
健康和版本查询、时间测量/同步以及 Wi-Fi 热点控制的最小可编译例子。`main()` 只输出目录
说明。

### `prism-configuration-api-examples`

该源码演示持久配置、自动/手动曝光、Camera/IMU 聚合与独立控制、Video ACK、LiDAR
启动和状态、LiDAR 网络管理、原始 `Frame` 命令、升级包检查和带确认保护的系统升级。
`main()` 不会执行这些修改操作。

### `prism-stream-api-examples`

该源码演示完整的 `ImuStream` 生命周期，以及仅点云、点云加 LiDAR IMU 两种
`LidarStream` 构造方式。每个例子都从同一个 Client 接收循环喂入帧，检查 `active()`，
并正常停止。

### `prism-parser-api-examples`

该源码使用每个公共名称 helper，并按对应 `FrameType` 分发全部公共 parser，其中同时
包含非 owning 和 owning Camera chunk parser。`main()` 是安全的空操作目录。

### `prism-windows-runtime-api-examples`

该 Windows 专用源码加载相邻 SDK DLL、验证 Runtime API v5、检查全部 45 个函数指针，
并为每个指针提供一个经编译检查的最小调用。运行时不需要设备，也不会修改设备。

四个 Linux/macOS 目录程序默认只输出说明，不会操作设备。Windows 目录程序会加载发布
DLL 并确认 45 个 Runtime API 入口全部存在，但不会打开或修改设备。CTest 会在编译后
运行这些目录程序。

仓库现在共有 8 个 example 源文件：Linux/macOS 编译 7 个目标，Windows 编译 2 个
目标，三平台矩阵合起来会编译全部源文件。如果以后新增 `examples/*.cpp` 却没有注册
CMake target，配置阶段会直接失败，因此 GitHub Actions 不会静默漏编示例。

## 自动化测试脚本

运行与 GitHub Actions 相同的发布文件校验、当前平台完整编译、目标清单检查和 CTest：

```bash
python3 scripts/test_all_examples.py --build-dir build-all-examples
```

Windows PowerShell：

```powershell
python scripts/test_all_examples.py --build-dir build-all-examples
```

只校验发布文件 SHA256：

```bash
python3 scripts/test_all_examples.py --verify-only
```

连接 USB 设备后，在离线测试完成后继续执行只读设备查询和 5 秒 Camera/板载 IMU 采集：

```bash
python3 scripts/test_all_examples.py \
  --build-dir build-all-examples --with-device
```

如果 LiDAR 已供电并连接，必须显式指定型号，才会再执行 5 秒点云和 LiDAR IMU 采集：

```bash
python3 scripts/test_all_examples.py \
  --build-dir build-all-examples --with-device \
  --capture-seconds 5 --lidar-model mid360
```

设备模式仍然是非破坏性的：不会同步时钟、保存配置、修改曝光/网络或安装固件。运行前请
关闭 Prism Viewer 和其他占用 USB 的程序。

脚本只支持发布动态库对应的 Ubuntu 22.04+ x86-64、macOS arm64 和 Windows x64。脚本
不会删除或清空指定的 build 目录。

## 退出码

所有示例成功时返回 0，参数、连接或 SDK 错误时返回 1。如果指定采集时间结束后没有
收到完整 Camera 帧组或 LiDAR 点云批次，对应流示例返回 3。

持久配置、曝光控制、Wi-Fi、LiDAR 网络管理、原始帧解析和系统升级等每个公开 SDK
接口的示例，请参阅 [逐接口 SDK 示例](../docs/interface-examples.zh-CN.md)。
