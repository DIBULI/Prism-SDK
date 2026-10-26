# 网口图像与 IMU 采集（SDK 1.3.0）

[示例目录](../README.zh-CN.md) · [English](README.md)

`network_camera_imu_capture.cpp` 使用 `Client::openNetwork()`，在同一个接收循环中获取多路 JPEG 图像和板载 IMU。支持 Linux x64/arm64、macOS arm64；Windows 包使用 RuntimeApi，此直接 Client 示例不参与 Windows 构建。

从仓库根目录编译：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target prism-network-camera-imu-capture --parallel
```

设备接入有线局域网，确认设备网关与 SDK 都为兼容的 1.3.0 版本。将下方 IP 替换为设备实际 IP；输出目录必须尚不存在，父目录必须已存在。先停止 Viewer、Web 或其他客户端的采集。

以下命令通过隐藏输入读取设备 Web 管理员密码（Bash）：

```bash
read -r -s -p 'Device Web password: ' PRISM_PASSWORD; echo
printf '%s\n' "$PRISM_PASSWORD" | ./build/examples/prism-network-camera-imu-capture \
  --host 192.168.1.100 --output capture-001 --seconds 10 --fps 20
unset PRISM_PASSWORD
```

密码从标准输入读取，不放入命令行参数或输出文件。网络接口是明文 TCP，仅用于可信局域网；默认端口 7443，可用 `--port` 修改。`--host` 接受数字 IPv4/IPv6，不解析域名。有线帧率上限为 20 FPS，Wi-Fi/未知链路为 10 FPS，具体设备模式可能更低；程序打印协商上限及实际启用帧率。默认 `--fps 0`、`--imu-rate 0` 使用设备配置；板载 IMU 可显式指定 `--imu-rate 800`，不会因网络相机限帧而降频。

输出内容：

- `cam<相机编号>_<帧编号>.jpg`：完整重组的 JPEG，可直接打开；未收到的相机不会用旧图补齐。
- `frames.csv`：相机编号、帧编号、时间戳、宽高、元数据是否有效、触发时间及图片文件名。
- `imu.csv`：传感器编号、样本编号、时间戳、同步标记、原始 flags、FSYNC、丢样本标记、三轴加速度（mg）、三轴角速度（mdps）、温度（毫摄氏度）。换算 SI 单位：加速度乘 `0.00980665` 得 m/s²；角速度乘 `π / 180000` 得 rad/s。

只有 IMU `timestamp_synced=1` 时，其 `timestamp_us` 才表示同步 UTC，否则是传感器板本地时间。图像保留 SDK 的时间戳和元数据有效性，不据此声称已 UTC 锁定，也不按图像帧号强配 IMU 样本。此例使用传统 JPEG 接口；原生无损 RAW12 见[网络及 RAW12 指南](../../docs/guides/network-raw.md)。

读循环先将帧交给 `ImuStream::handleFrame()`，再将视频分片和元数据交给 `CameraFrameAssembler`。完整帧组、过期帧组都发送 ACK；部分帧组仅保存其中完整的图像。即使没有新包也执行过期检查，读超时继续等待，断线和写盘失败报错。Ctrl+C 或到时会尝试停止采集并关闭文件。停止前仍未完成的帧组不保存。退出码 0 表示图像和 IMU 都有数据，3 表示至少一种完全没有数据，1 表示参数或运行错误。

这是便于理解的同步写盘示例，慢磁盘可能造成丢帧，不保证无损持续录制。需要高吞吐时，可在处理完整图像/IMU 回调处接入有界写盘队列，并单独监控丢帧和样本间隔。

仅检查连接和设备信息可运行 `prism-network-connect numeric-IP [port]`，同样从标准输入读取密码。两个程序的 `--help` 都不连接设备。
