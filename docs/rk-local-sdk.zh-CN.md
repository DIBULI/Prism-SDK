# RK-local SDK 指南

## 在 RK 本机编译和运行示例

```sh
cmake -S examples/rklocal -B build/rklocal -DCMAKE_BUILD_TYPE=Release
cmake --build build/rklocal --parallel
ctest --test-dir build/rklocal --output-on-failure
# 10 秒四路相机 + IMU0，不写图片到磁盘：
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 - 1
# 可选：第一组四张 JPEG 保存到一个新目录：
build/rklocal/prism-rklocal-capture /run/prism/stream.sock 10 ./first-jpegs 1
# 只查 GNSS，10 次，查询间隔 100 ms，不采集/校时：
build/rklocal/prism-rklocal-gnss-status /run/prism/stream.sock 10
```

交叉编译增加 `-DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/aarch64-linux-gnu.cmake`，
必要时设置 `-DPRISM_AARCH64_CROSS_PREFIX=/路径/aarch64-none-linux-gnu-`。
仅需 C、pthread 和随包 ARM64 静态库，不依赖 USB/OpenSSL。

`examples/rklocal_capture.c` 默认一个 IMU，使用 Agent 配置的频率（目前 800 Hz）。
打印首个 IMU 值、四路图像尺寸/时间戳/曝光及计数。图像是完整 JPEG 缓冲区，
IMU 加速度单位 mg、角速度 mdps、时间戳微秒。两颗 IMU 都在时再将末尾参数改为 2。
Ctrl-C 停止采集；空图像/IMU 输出或采集错误会返回失败。

`examples/rklocal_gnss_status.c` 展示定位、卫星、DOP、新鲜度以及 PPS 有效性、
脉宽、每 PPS 后首条 RMC 延迟；没有实时 GNSS 时报告无 fix/PPS 无效并不代表
SDK 查询失败。不修改配置，也不自动校时。

RK-local SDK 是供应用直接运行在 Prism RK3576 上的 Linux C API。它通过
`/run/prism/stream.sock` 连接 `prism-agent`。Sensor Board UART、Camera
CSI/V4L2、时间同步和采集会话仍全部由 Agent 独占管理。

## 包含文件

- 公共头文件：`include/prism/rklocal_sdk.h`
- ARM64 静态库：`runtime/linux-arm64/libprism_rklocal_sdk.a`
- CMake 导入目标：`cmake/PrismRkLocalSdk.cmake`
- 示例：`examples/rklocal_capture.c`

SDK 版本为 `1.1.0`，local protocol 为 `1`，必须与 Agent `1.1.0` 完成严格版本
握手。它不是 USB Host SDK，不支持 Windows、macOS 或 Linux x86-64。

## CMake 接入

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_rk_app LANGUAGES C)

include("/opt/Prism-SDK/cmake/PrismRkLocalSdk.cmake")
add_executable(my_rk_app main.c)
target_link_libraries(my_rk_app PRIVATE Prism::RkLocal)
```

导入目标会自动提供公共 include 路径、ARM64 静态库和 pthread 依赖。最终程序不需要
额外部署 RK-local SDK 动态库。

## 连接与所有权

```c
prism_rklocal_config_t config;
prism_rklocal_client_t *client = NULL;

prism_rklocal_config_default(&config);
if (prism_rklocal_open(&client, &config) != PRISM_RKLOCAL_OK)
  return 1;
/* 使用 client。 */
prism_rklocal_close(client);
```

同一时间只有一个本地 client 可以控制采集。SDK 内部维护接收线程、keepalive 线程和
有界 IMU/图像队列；消费者过慢时丢弃最旧的本地数据，不阻塞 Agent。

## Camera 与 IMU

`prism_rklocal_start()` 启动 Camera 和板载 IMU 聚合采集。只安装 IMU0 的设备将
`imu_sensor_count` 设为 `1`，否则使用 `2`。通过 `prism_rklocal_read_imu()` 读取
IMU，通过 `prism_rklocal_read_frame_set()` 读取完整四 Camera JPEG。每次成功读取
frame set 后必须调用 `prism_rklocal_frame_set_release()`。

## GPS/GNSS 与 CORS

`prism_rklocal_get_gnss_status()` 上报定位、位置、卫星数、DOP、NMEA 新鲜度、授时
状态、PPS 有效性和 PPS 高电平宽度，不暴露内部 RK-PPS 相位质量数据。

`prism_rklocal_get_rtk_correction_status()` 上报当前改正源、检测到的
RTCM2.x/RTCM3.x 格式、字节数、观测 epoch、解算数和 decoder 错误。

本地应用可通过 begin/send/end 接口向 Agent 传入原始 RTCM2.x 或 RTCM3.x 字节。
NTRIP endpoint 选择、认证、重连和 GGA 上传由应用负责。

## RTK 导航

`prism_rklocal_get_rtk_navigation()` 查询当前导航快照，
`prism_rklocal_read_rtk_navigation()` 等待最新实时事件。结果会同时保留原始 RTKLIB
位置，并单独上报启用 dynamics 的平滑位置、状态切换/跳变门控和重置计数。两路结果
都不会使用 SINGLE fallback。

## 错误与退出

所有函数返回 `prism_rklocal_result_t`。使用 `prism_rklocal_last_error()` 获取 client
保存的最近一次诊断。退出时先对活动采集调用 `prism_rklocal_stop()`，再调用
`prism_rklocal_close()`。

socket 必须存在，而且应用用户需要读写权限。版本不匹配、已有采集 owner、Agent
重启、socket 关闭和超时都是应用应明确报告的正常运行错误。
