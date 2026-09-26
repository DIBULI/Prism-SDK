# TimeSync modes / TimeSync 接口模式

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

## Selection and persistence / 选择与持久化

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
See [RTK-module control](rtk-module-control.md) for configuration and start/stop.

## Input / 输入

Receive external PPS and valid RMC time labels. PPS detection, pulse validity,
valid RMC and external UTC synchronization are separate states. A valid pulse
alone is not UTC lock. `gnssReceptionStatus()` distinguishes no input, stale input
and rejected NMEA; `gnssTimingStatus()` reports accepted timing data.

## Output / 输出

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
