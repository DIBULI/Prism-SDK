# Time synchronization and TimeSync modes / 时间同步与接口模式

[Documentation index](../README.md) · [中文目录](../README.zh-CN.md)

- [Time model, validity and manual synchronization / 时间模型、有效性与手动校时](#time-model)
- [TimeSync input, output and RTK mode / 接口模式与持久化](#port-modes)

CORS accounts and receiver positioning controls are in
[GNSS / RTK](gnss-rtk.md#cors-control)，不属于手动校时接口。

<a id="time-model"></a>

## Time model and synchronization APIs / 时间模型与校时接口

适用于 Agent / Host SDK / RK-local SDK **1.2.0** 与 Sensor Board **0.4.27**。
SDK 连接时校验版本，连接和状态查询不会自动校时。

### 1. 当前时间模型

Sensor Board 始终是设备时间线的主控。外部 GNSS PPS 与有效 RMC 可校准该时间线；
无外部授时时使用板内时间基准，已建立 UTC 后可保持运行。RK 和 Ethernet PTP
跟随 Sensor Board，不反向作为板端 PPS 来源。保持运行不等于外部 UTC 仍然锁定。

界面只区分内部授时、外部授时；不要直接显示底层来源枚举。
无 UTC 时，本地连续时间不能伪装成真实 UTC。判断数据时还须检查每条时间戳有效位。

### 2. 全局与外部授时分别查询

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

### 3. Host 校时与 RK-local 的区别

`synchronizeTimeNtpLike()` 测量设备相对调用端系统时钟的偏差，不修改时钟。
`synchronizeSystemTime()` 显式使用调用端系统时间作为 UTC 输入，经 Sensor Board
建立时间基准，再由 Agent 验证 RK 和 PHC，最后复测残差。RTC 为可选硬件。

RK-local 进程与 RK 共用系统时钟，不能靠 `synchronizeSystemTime()` 获得外部 UTC。
已获得独立 UTC 的 RK-local 应用使用：

```cpp
// RK-local only; utc_us 是函数入口时刻的外部 UTC 微秒值。
const auto result = client.setDeviceTime(utc_us, 35000);
```

SDK 按单调时间推进传入标签，通过 Agent 设置 Sensor Board 并验证后续时间映射。
Host 使用 `synchronizeSystemTime()`；两端该时间输入方式不完全相同。

- 校时前停止相机、IMU、LiDAR 及其他活动数据流。
- 外部 GNSS 已授时时拒绝手动校时。
- 校时不能绕过 Sensor Board 直接置位 IMU 的 UTC 有效标志。
- `verified` 为成功条件；超时或未确认不代表已回滚。
- 不应在每次连接时自动执行校时。

刷新后的 1.2.0 Host SDK 在空闲校时期间暂缓发送 USB 心跳，成功返回或异常退出时
自动恢复发送，保持原有心跳开关和发送间隔，不要求应用通过断开重连恢复心跳。
不要为了绕过这一问题关闭采集时的心跳保护。校时失败仍需处理，USB 真正断开时仍需重连；
该修复不保证 UTC 保持，也不处理时间回退。RK-local/Web 不使用 Host USB 心跳线程。

配套 Agent 等待三个新鲜 PPS 对齐样本，且在 PHC 验证后确认统一时间基准仍就绪，
才报告校时成功。SDK 将零值、倒退或不可能的时间跨度视为无效样本，最多六秒只重读，
不会将 `0` 当作 1970 年时间下发二次校时；验证中出现新的时间纪元则明确失败。
Host/RK-local 为 TIME_SET 保留至少 35 秒等待窗口，不放宽 PPS 对齐精度和新鲜度要求。
部署本轮匹配的 Agent/SDK；不需要更新 Sensor Board，也不改变时间来源或 UTC 保持策略。

The refreshed 1.2.0 Host SDK pauses USB heartbeat writes during idle calibration
and releases that pause on both success and exception, preserving the caller's
enable state and interval. Do not disable the capture watchdog as a workaround.
Calibration errors remain errors, and a genuinely disconnected USB device still
requires reconnection. This fix does not change UTC retention or clock rollback.

### 4. TimeSync 接口模式

`GnssInput`、`PpsNmeaOutput` 和 `Rtk` 通过独立持久化接口选择。
手动保存的模式优先恢复。模式切换不等于更换设备时间线主控。
具体输出行为及防止输出冲突的要求见 [TimeSync 模式说明](#port-modes)。

### 5. 接收循环

Host 同一 Client 只用一个接收循环分发 Heartbeat、Camera、IMU 和 LiDAR；
控制调用也应串行化，不另开一个线程竞争 `readFrame()`。
RK-local 可使用 `readImu()`、`readFrameSet()`，并遵循
[RK-local 资源和线程约束](../reference/rk-local.zh-CN.md)。

<a id="port-modes"></a>

## TimeSync modes and persistence / 接口模式与持久化

Current baseline: Agent / Host SDK / RK-local SDK **1.2.0**, Sensor Board **0.4.27**.
Use matching headers and libraries containing these APIs.

Sensor Board owns the device timeline in all three modes. RK and Ethernet PTP
follow that timeline. Mode selection is not proof of UTC synchronization:
read `gnssTimingStatus().time_synced` and timestamp validity flags.

| C++ mode | 用途 / Purpose |
| --- | --- |
| `GnssInput` | 外部 PPS + NMEA 输入 / external timing input |
| `PpsNmeaOutput` | 对外输出 PPS + 时间 NMEA / external timing output |
| `Rtk` | RTK-module 状态、版本、CORS 配置及定位启停 / RTK-module control |

### Selection and persistence / 选择与持久化

Host `prism::Client` and RK-local `prism::rklocal::Client` expose the same methods:

```cpp
auto status = client.timeSyncPortStatus();
// Stop acquisition and use wiring appropriate to the selected mode first.
status = client.setTimeSyncPortMode(prism::TimeSyncPortMode::Rtk);
```

Stop camera, IMU and LiDAR transfer before changing modes. Agent verifies
hardware readback and persists the selection on RK. Check `persisted`, `applied`,
`sensor_board_online` and `error_code`; `mode` alone is not hardware confirmation.
`SensorBoardMaster` is an alias of `GnssInput`.

手动保存的模式在 Agent 重启后优先恢复，不被启动自动探测覆盖。未保存手动模式时，
启动探测先等待外部 PPS/NMEA，再按设备能力检测 RTK-module。
FPGA 复位时先释放外部接口，Agent 随后恢复配置。

Selecting RTK mode neither starts CORS nor authorizes GGA transmission.
接收机未就绪、GNSS 未定位、CORS 待应用，不等于接口模式应用失败。
See [RTK-module control](gnss-rtk.md#cors-control) for configuration and start/stop.

### Input / 输入

Receive external PPS and valid RMC time labels. PPS detection, pulse validity,
valid RMC and external UTC synchronization are separate states. A valid pulse
alone is not UTC lock. `gnssReceptionStatus()` distinguishes no input, stale input
and rejected NMEA; `gnssTimingStatus()` reports accepted timing data.

### Output / 输出

Disconnect external TX/PPS drivers before selecting output. Two transmitters
must not drive the same signal. The connector uses 3.3 V logic, not RS-232.

- PPS: 1 Hz, 100 ms high.
- GPRMC: one checksummed sentence per PPS, UTC/date labels that PPS edge.
- UART: configured GNSS baud, default 460800, 8N1.
- Position, speed and course are synthetic zero placeholders for timing only.
  **Never use them as real GNSS position, CORS rover position or ground truth.**
- Without external/host UTC, the emitted epoch-relative timeline is not known UTC.
  Holdover after external synchronization is lost is not an external UTC lock.
- Returning to input releases the outputs and aborts a partial sentence.

```cpp
// Only after stopping acquisition and disconnecting external transmitters:
client.setTimeSyncPortMode(prism::TimeSyncPortMode::PpsNmeaOutput);
// Before reconnecting external transmitters:
client.setTimeSyncPortMode(prism::TimeSyncPortMode::GnssInput);
```

CLI: Host `prism-timesync`, RK-local `prism-rklocal-timesync`.
No arguments or `--status` only queries. `--input` selects input;
`--output --external-transmitters-disconnected` explicitly selects output.
`--help` never connects.
