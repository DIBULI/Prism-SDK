# GNSS 接收、NMEA 识别与授时状态

本页描述开发中的新增接口，尚未替换 GitHub 已发布的 SDK 包。

## 接口与兼容性

**不修改原接口。** `client.gnssTimingStatus()`、`prism::GnssTimingStatus` 的字段、
布局和含义保持原样。时间查询命令仍为 0x3a/0xb6，报文仍为 v5、104 字节；
原 `RuntimeApi` 仍为 ABI 12，原函数表不增加成员。

新增独立查询，两种 SDK 使用同一个返回类型和方法签名：

```cpp
// Host: prism::Client
// RK-local: prism::rklocal::Client
prism::GnssReceptionStatus gnssReceptionStatus();
```

类型位于 `prism/usb/gnss_reception.hpp`，各自 Client 头文件已包含此类型。
不需要改变已有调用；只有要用新诊断的应用才需要增加新接口调用并链接新库。
新增方法不改变 Client 实例布局，也不改公开 SDK/Agent 版本号。

新命令为 0x3d/0xb8、独立协议 v1、56 字节、小端编码。依次为 version(u16)、
size(u16)、flags(u32)、raw_age_ms(u32)、nmea_sentence_age_ms(u32)，随后五个 u64
计数依次为 raw_byte_count、nmea_sentence_count、nmea_rejected_count、
uart_frame_error_count、fifo_overflow_count。flags 位 0..5 依次对应下面六个布尔字段。

| 字段 | 含义 |
| --- | --- |
| `sensor_board_online` | Agent 查询时 Sensor Board 是否在线 |
| `reception_available` | Agent 的接收诊断源可用；false 不能解释为没有收到字节 |
| `raw_data_seen` | 本次接收会话已收到 Sensor Board 转发的外部 GNSS UART 字节，包括 NMEA、RTCM 或其他数据 |
| `raw_data_fresh` | 最近 2000 ms 内仍收到字节 |
| `nmea_sentence_seen` | 已收到地址及校验和有效的标准 NMEA 报文，不要求定位有效；RMC 的 V 状态也计入 |
| `nmea_sentence_fresh` | 最近 2000 ms 内仍收到有效 NMEA |
| `raw_age_ms` | 最近收到字节距当前的毫秒数 |
| `nmea_sentence_age_ms` | 最近收到有效 NMEA 距当前的毫秒数 |
| `raw_byte_count` | 累计转发至 Agent 接收解析器的字节数 |
| `nmea_sentence_count` | 累计地址、校验和有效的 NMEA 报文数 |
| `nmea_rejected_count` | 进入行解析器后被拒绝的候选报文数，不等于所有坏字节数 |
| `uart_frame_error_count` | Sensor Board 转发的 UART 帧错误标记累计次数 |
| `fifo_overflow_count` | Sensor Board 转发的 FIFO 溢出标记累计次数 |

年龄使用单调时钟，`UINT32_MAX` 表示未收到或年龄饱和；`seen` 是历史状态，
`fresh` 是当前活跃状态。计数不持久化，会随相应 Agent/Sensor Board 接收会话重置。
原始字节不是管脚电平活动；错误计数只反映已转发的标记，没有字节时错误标记可能尚未送达。

原接口的 `nmea_seen`、`nmea_age_ms`、`nmea_update_count` 仍描述 GGA/GSA 定位质量数据。
只有新的 RMC/RTCM 不会刷新旧 GGA/GSA 位置。原接口的 `pps_detected`、`pps_valid`、
`time_synced` 仍分别代表 PPS 检测、脉宽资格及外部授时锁定。不增加 RK-PPS 精度公共接口。

两个接口是独立快照，不保证同一采样时刻。UART、NMEA、定位、PPS、UTC 锁定不能互相替代。

## Windows 动态运行库

原导出 `prism_usb_sdk_get_runtime_api(12)` 及函数表不变。
新增可选导出 `prism_usb_sdk_get_gnss_reception_api(1)`，返回
`prism::GnssReceptionRuntimeApi`，仅包含版本、大小、`gnss_reception_status` 函数指针。
定义见 `prism/usb/gnss_reception_runtime_api.hpp`。

先正常加载旧表，再按需查找新导出；缺失新导出只表示不支持接收诊断。
新 Agent 不支持查询、新库未部署、接收诊断源不可用是不同情况，不应伪装成零接收。
使用头文件+库的常规链接方式时，调用新方法必须链接包含该方法的新库；
二进制兼容仍要求原有编译器、架构和 C++ 运行库条件一致。

Viewer 将新查询与时间查询分别处理。诊断查询失败时显示不可用并停止本次连接的诊断轮询，
重新连接后重试，原来的定位、PPS、授时显示继续使用旧接口。旧录像缺少诊断也显示不可用。

## 调用示例

```cpp
const auto timing = client.gnssTimingStatus();  // 原来的调用保持不变
try {
    const auto rx = client.gnssReceptionStatus();  // Host / RK-local 一样
    if (!rx.reception_available) {
        // 没有可用接收诊断，不表示 UART 没有数据。
    } else if (!rx.raw_data_fresh) {
        // 看 raw_data_seen 区分从未收到 / 已停止更新。
    } else if (!rx.nmea_sentence_fresh) {
        // 有字节但没有近期有效 NMEA，可能仅 RTCM 或格式/校验错误。
    } else if (!timing.time_synced) {
        // NMEA 正常，但外部 UTC 尚未锁定；单独检查 PPS 和 RMC 配对。
    }
} catch (const std::exception& error) {
    // 单独报告接收诊断查询失败；保留原 timing 查询结果。
}
// 位置可用性仍由 timing.nmea_fix_valid/nmea_position_valid/nmea_age_ms 判断。
```

## Sensor Board 配套行为

外部输入模式无论 UTC 是否锁定都转发 NMEA/RTCM 及 UART 错误标记。
输出模式仍禁止接收自身输出。20 ms PPS 最低脉宽、RMC 校验、0..800 ms 配对
和 UTC 连贯性检查不放宽。

解除转发限制需要重新构建并刷写配套 Sensor Board BOOT.BIN；仅更新 Viewer/SDK/Agent
不能恢复旧固件已屏蔽的数据。本次代码修改不表示已经更新设备，也不证明
“插拔 UART 后才恢复”的物理连接或启动时序根因已经修复。
