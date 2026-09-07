# SDK 1.1.0：GNSS、CORS 与 RTK

必须成套使用 1.1.0 头文件和运行库，以及 Agent 1.1.0，不兼容旧 Agent。
Windows 使用 Runtime API 12（MSVC C++ ABI 表，不是跨编译器 C ABI）。
RK-local C11 API 通过设备本机 `/run/prism/stream.sock` 访问 Agent，不直接读 UART。
完整的 Host / Windows / RK-local 接口对应表见 [英文接口说明](gnss-rtk.md)。

## GNSS/PPS 状态

Host 用 `gnssTimingStatus()`，RK-local 用 `prism_rklocal_get_gnss_status()`。
`time_synced` 表示实时外部授时锁定；仅收到有效 PPS 不等于 UTC 已同步。
使用经纬度、卫星数、DOP 时同时检查 fix/position/DOP 有效位以及
`nmea_age_ms`、`nmea_update_count`。没有新报文时不能把缓存坐标当成最新定位。

- 经纬度：有符号度数 × 1e7。
- GGA 海拔和大地水准面分离：毫米；椭球高 = 海拔 + 分离值。
- DOP：×1000；UTC：当天毫秒数。
- PPS：`pps_detected`、`pps_valid`、`pps_high_width_us`、`pps_min_high_us`。
- `offset_fresh` 有效时，`message_pps_offset_us` 是对应 PPS 后第一条有效 RMC
  的延迟，范围 0..800000 µs。过期时不要展示为当前延迟。

SDK 不公开 RK-PPS 精度诊断接口，但保留 PPS 有效性、脉宽等状态。
Host 可通过 `DeviceConfiguration` 的 GNSS 配置字段设置接收波特率，包括
115200、460800、921600 等常用档位。当前模式固定为 Sensor Board 主时钟；
旧 `PpsNmeaOutput` 输出模式会被 Agent 拒绝。

## CORS / 原始 RTCM

SDK **不负责 CORS 登录**。应用负责 NTRIP 鉴权、断线重连与上传 GGA，再把收到的
原始 RTCM2.x/RTCM3.x 数据通过 begin/send/end 接口传入，每块不超过 16 KiB。
`rtkCorrectionStatus()` / `prism_rklocal_get_rtk_correction_status()` 上报检测到的
格式、来源、字节数、解算数和错误计数；其他格式暂不支持。
这些数据交给 RK 内的解算器，不发给 UM960 或 Sensor Board。

GGA 使用设备当前有效 GNSS 位置生成，不伪造 fix 或把过期位置当成新定位。
算法回放时分别记录 rover 原始观测与 CORS 基站改正流，并保存到达时间。
Host 的 `startRoverRtcm()` / `parseRoverRtcmChunkView()` 输出完整、CRC 验证的
RTCM3 帧，不含 NMEA；视图依赖原 Frame 内存，异步保存前先复制。
RK-local 本次提供 GNSS/RTK 状态与改正流输入，尚未暴露 rover 原始流输出接口。

## RTK 原始解与平滑解

`rtkNavigationStatus()` / `prism_rklocal_get_rtk_navigation()` 同时报告两套数据。
用 `solution_valid` 判断原始解，`smoothed_position_valid` 判断独立平滑解。
两者分别包含 epoch、FIX/FLOAT 状态、经纬度、椭球高及 E/N/U 标准差；
高度与标准差单位为米，经纬度单位为度。不能把标准差当作米制位置。
ENU 位置需要明确共同原点后自行转换。

RTK 失败时不会用 SINGLE 解覆盖 RTK 位置，且会上报质量门控、跳变/断档重置与计数。
显示“几秒前的解”时用设备 UTC 减去对应 epoch；有效但很旧的解不代表实时结果。
GNSS 输入 10 Hz 不保证每个历元都成功产生 RTK 解。

RK-local 的 `prism_rklocal_read_rtk_navigation()` 等待最新事件；慢消费者只保留
最新未读快照，不应阻塞采集。

## 时间与示例

连接 SDK 不自动校时。Sensor Board 始终掌管传感器时间，RK 跟随其 PPS/NMEA 并向
以太网提供 PTP。GPS 授时成功时 Agent 拒绝主机校时。GNSS 暂时失锁不代表传感器
失去共同时间基准。

Host 示例为 `build/examples/prism-gnss-rtk-status`；RK 端使用
[本地采集和 GNSS 查询例子](rk-local-sdk.zh-CN.md)。Windows 示例验证全部 57 个
Runtime API 12 指针，编译检查目录包含新接口调用，不自动执行设备修改。
