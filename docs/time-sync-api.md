# Prism 时间同步接口（1.2.0）

适用于 Agent / Host SDK / RK-local SDK **1.2.0** 与 Sensor Board **0.4.27**。
SDK 连接时校验版本，连接和状态查询不会自动校时。

## 1. 当前时间模型

Sensor Board 始终是设备时间线的主控。外部 GNSS PPS 与有效 RMC 可校准该时间线；
无外部授时时使用板内时间基准，已建立 UTC 后可保持运行。RK 和 Ethernet PTP
跟随 Sensor Board，不反向作为板端 PPS 来源。保持运行不等于外部 UTC 仍然锁定。

界面只区分内部授时、外部授时；不要直接显示底层来源枚举。
无 UTC 时，本地连续时间不能伪装成真实 UTC。判断数据时还须检查每条时间戳有效位。

## 2. 全局与外部授时分别查询

```cpp
const auto info = client.deviceInfo();
const auto timing = client.gnssTimingStatus();
const auto reception = client.gnssReceptionStatus();
// info.sensor_board_online: 板端在线
// info.sensor_board_time_synced: 板端 UTC 状态
// timing.time_synced: 实时外部 GNSS 授时锁定
// reception: UART/NMEA 接收诊断，不代表定位或授时成功
```

`pps_detected`、`pps_valid`、`time_synced` 含义不同。
PPS 存在且脉宽合法，不保证 NMEA 有效，也不保证 UTC 锁定。
`message_pps_offset_us` 仅在 `offset_fresh` 时可用于显示对应 PPS 后第一条有效 RMC 的延迟。
这些查询是独立快照，不保证同一采样时刻。

| 字段 | 用途 |
| --- | --- |
| `DeviceInfo.sensor_board_online` | 板端在线状态 |
| `DeviceInfo.sensor_board_time_synced` | 板端 UTC 状态 |
| `DeviceInfo.imu_time_synced_mask` | 各 IMU 最近状态的时间同步位 |
| `GnssTimingStatus.time_synced` | 实时外部授时锁定 |
| `ImuSample.timestamp_synced` | 当前样本时间戳是否可作为 UTC 使用 |
| `ImuSample.fsync_event` / `fsync_delay_valid` | 当前样本的 FSYNC 事件与延迟有效性 |

外部 GNSS、内部时间线、IMU FSYNC 和每条样本 UTC 有效性分别判断。
不要把 `fsync_event=true` 等同于外部授时成功，也不要从外部 GNSS 未锁定推断 IMU 未同步。

Heartbeat 只提供 RK 系统时间，不携带其他设备状态：

```cpp
const auto frame = client.readFrame(1500);
if (frame.type == prism::FrameType::Heartbeat) {
  const uint64_t rk_utc_us = prism::parseHeartbeat(frame).rk_system_time_us;
}
```

## 3. Host 校时与 RK-local 的区别

`synchronizeTimeNtpLike()` 测量设备相对调用端系统时钟的偏差，不修改时钟。
`synchronizeSystemTime()` 显式使用调用端系统时间作为 UTC 输入，经 Sensor Board
建立时间基准，再由 Agent 验证 RK 和 PHC，最后复测残差。RTC 为可选硬件。

RK-local 进程与 RK 共用系统时钟，不能靠 `synchronizeSystemTime()` 获得外部 UTC。
已获得独立 UTC 的 RK-local 应用使用：

```cpp
// RK-local only; utc_us 是函数入口时刻的外部 UTC 微秒值。
const auto result = client.setDeviceTime(utc_us, 25000);
```

SDK 按单调时间推进传入标签，通过 Agent 设置 Sensor Board 并验证后续时间映射。
Host 使用 `synchronizeSystemTime()`；两端该时间输入方式不完全相同。

- 校时前停止相机、IMU、LiDAR 及其他活动数据流。
- 外部 GNSS 已授时时拒绝手动校时。
- 校时不能绕过 Sensor Board 直接置位 IMU 的 UTC 有效标志。
- `verified` 为成功条件；超时或未确认不代表已回滚。
- 不应在每次连接时自动执行校时。

## 4. TimeSync 接口模式

`GnssInput`、`PpsNmeaOutput` 和 `Rtk` 通过独立持久化接口选择。
手动保存的模式优先恢复。模式切换不等于更换设备时间线主控。
具体输出行为及防止输出冲突的要求见 [TimeSync 模式说明](timesync-port.md)。

## 5. 接收循环

Host 同一 Client 只用一个接收循环分发 Heartbeat、Camera、IMU 和 LiDAR；
控制调用也应串行化，不另开一个线程竞争 `readFrame()`。
RK-local 可使用 `readImu()`、`readFrameSet()`，并遵循
[RK-local 资源和线程约束](../rk-local-sdk/README.zh-CN.md)。
