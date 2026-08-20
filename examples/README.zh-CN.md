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
v4 后再调用接口。

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

## 退出码

所有示例成功时返回 0，参数、连接或 SDK 错误时返回 1。如果指定采集时间结束后没有
收到完整 Camera 帧组或 LiDAR 点云批次，对应流示例返回 3。

持久配置、曝光控制、Wi-Fi、LiDAR 网络管理、原始帧解析和系统升级等每个公开 SDK
接口的示例，请参阅完整的 [SDK 开发文档](../docs/development-guide.zh-CN.md)。
