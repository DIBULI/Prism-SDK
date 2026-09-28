# GNSS / RTK: results, CORS control and diagnostics / 结果、配置与诊断

[Documentation index](../README.md) · [中文目录](../README.zh-CN.md)

Results, account configuration, receiver controls and diagnostics are collected
here. 本页集中说明定位、账号、启停、版本及接收诊断，不需要在多个专题文件间跳转。

- [Receiver results / English](#receiver-results) · [接收机结果 / 中文](#receiver-results-zh)
- [CORS configuration and RTK start/stop / CORS 配置与启停](#cors-control)
- [Module firmware versions / 模块固件版本](#module-versions)
- [Reception diagnostics / 接收诊断](#reception-diagnostics)

TimeSync input/output selection and clock validity are documented separately in
[time synchronization / 时间同步](time-sync.md).

<a id="receiver-results"></a>

## Receiver results (English)

Agent forwards the receiver's measurements and results. The SDK does not run
an RTK solver, manufacture FIX states, or provide a separate smoothed position.

| Purpose | Host and RK-local API |
| --- | --- |
| Live GGA, fix, satellites, PPS/timing freshness | `gnssTimingStatus()` |
| UART/framing/parse diagnostics | `gnssReceptionStatus()` |
| Receiver reports for sky and positions | `gnssObservations(cursor, session)` |
| Module link, 4G, CORS counters and execution state | `timeSyncRtkStatus()` |
| Application / bootloader versions | `timeSyncRtkVersions()` |
| Saved CORS endpoint; password never returned | `timeSyncCorsConfiguration()` |
| Save a full replacement CORS account | `saveTimeSyncCorsConfiguration(config)` |
| Explicit RTK start / stop | `startRtk(options)` / `stopRtk()` |

Use `prism::gnss_plot::Model` from `<prism/usb/gnss_plot.hpp>` with observation
batches. `model.gnss` is GGA; `model.rtk` is the independent ADRNAV receiver
solution. Keep them separate. Check `valid` and freshness against the batch's
device monotonic time. `session` changes reset the model, and `gap` indicates
missing history. GGA UTC is time-of-day; ADRNAV epoch is GPS week/milliseconds,
not a host Unix timestamp. Preserve the original report when full timing matters.

Receiver standard deviations are optional uncertainty estimates in metres, not
a guaranteed confidence percentage. Missing uncertainty remains unavailable.
An absent/stale result is not a transport failure and must not be shown as live.

`startRtk` confirms the requested control transition, not satellite lock or FIX.
Saved, applied, connected and positioning are different states. See
[CORS/RTK control](#cors-control), [TimeSync modes](time-sync.md#port-modes),
[GNSS diagnostics](#reception-diagnostics) and the
[read-only example](../../examples/gnss_rtk_status.cpp).

For English usage instructions, see the source comments in
[RTK control](../../examples/rtk_module_control.cpp) and [continuous positions](../../examples/rtk_position.cpp).

<a id="receiver-results-zh"></a>

## 接收机结果（中文）

Agent 转发接收机数据，不再内置 RTK 解算器；SDK 不生成 FIX，也不提供独立平滑位置。

| 功能 | Host 与 RK-local 接口 |
| --- | --- |
| 实时 GGA、定位、卫星数和 PPS/授时 | `gnssTimingStatus()` |
| 串口接收、帧和解析诊断 | `gnssReceptionStatus()` |
| 天空图及接收机定位报文 | `gnssObservations(cursor, session)` |
| 模块连接、4G、CORS 计数和控制执行状态 | `timeSyncRtkStatus()` |
| 应用/引导程序版本 | `timeSyncRtkVersions()` |
| 已存 CORS 配置，不返回密码 | `timeSyncCorsConfiguration()` |
| 完整替换并保存 CORS 配置 | `saveTimeSyncCorsConfiguration(config)` |
| 明确启停 RTK | `startRtk(options)` / `stopRtk()` |

`<prism/usb/gnss_plot.hpp>` 中的 `prism::gnss_plot::Model` 可解析报文批次。
`model.gnss` 来自 GGA，`model.rtk` 来自独立 ADRNAV 接收机结果，两者应分开展示。
必须检查 `valid`，并用批次中的设备单调时钟判断新鲜度；`session` 改变时模型复位，
`gap` 表示历史数据缺失。GGA UTC 为日内时间，ADRNAV 历元为 GPS 周/毫秒，
不是主机 Unix 时间戳；需要完整时间语义时保留原始报文。

接收机标准差是可选的米级不确定度估计，不是保证准确的置信百分比。
缺失值应显示“未提供”；缓存结果不能当作实时定位。
`startRtk` 成功仅确认控制执行，不表示卫星已定位或已获得 FIX。
保存成功、模块应用成功、CORS 连通和定位成功必须分开判断。

RTK 示例使用说明见源码顶部的英文注释：[CORS 与启停](../../examples/rtk_module_control.cpp)、
[连续读取结果](../../examples/rtk_position.cpp)。
详见 [CORS/RTK 控制](#cors-control)、[TimeSync](time-sync.md#port-modes)、
[接收诊断](#reception-diagnostics)和[只读示例](../../examples/gnss_rtk_status.cpp)。

<a id="cors-control"></a>

## CORS configuration and RTK start/stop / CORS 配置及 RTK 启停

English usage instructions are maintained in the source comments:
[RTK control](../../examples/rtk_module_control.cpp) and [continuous positions](../../examples/rtk_position.cpp).

Agent / Host SDK / RK-local SDK: **1.2.0**. Use matching current headers and
libraries. These wrappers use existing Agent RTK-module commands, without a new
wire protocol or a Sensor Board firmware change.

### Shared C++ API / 两种 SDK 一致的业务接口

Host: `<prism/usb_sdk.hpp>`, `prism::Client` over USB.
RK-local: `<prism/rklocal_sdk.hpp>`, `prism::rklocal::Client` over the Agent socket.
Both compile the same implementation and use the same types/defaults:

```cpp
TimeSyncCorsStatus timeSyncCorsConfiguration();
TimeSyncCorsStatus saveTimeSyncCorsConfiguration(const TimeSyncCorsConfiguration&);
TimeSyncRtkStatus startRtk(const RtkStartOptions& options = {});
TimeSyncRtkStatus stopRtk(uint32_t timeout_ms = 20000);
TimeSyncRtkStatus timeSyncRtkStatus();
TimeSyncRtkVersions timeSyncRtkVersions();
```

No call automatically changes TimeSync mode. Select `TimeSyncPortMode::Rtk`
while acquisition is stopped. Opening/reading status never starts RTK/CORS.

### Configure / 配置

```cpp
// Either connected Client type; account and password supplied by application UI.
prism::TimeSyncCorsConfiguration cfg;
cfg.enabled = true;
cfg.ip = "192.0.2.10"; // Example only: replace with your service's IPv4.
cfg.port = 8002;
cfg.mountpoint = "MOUNTPOINT";
cfg.username = account_from_ui;
cfg.password = password_from_ui;
const auto saved = client.saveTimeSyncCorsConfiguration(cfg);
if (saved.configuration_saved && !saved.configuration_applied) {
  // Display "Saved; waiting for module readiness", then periodically query.
}
```

This is a **full replacement**, not a field patch. Supply the password again for
an enabled account. Blank password does not mean "keep existing".
`TimeSyncCorsConfiguration{}` explicitly disables CORS and clears account fields.
An empty query never authorizes an automatic clear.

账号和密码由 Agent 持久化在 RK，模块连接并就绪后自动下发。保存不要求已经定位或联网。
保存不启动 RTK、不授权发送 GGA；更新运行中的配置可能使模块停止并重新应用，应查询状态。
`configuration_saved`、`configuration_applied`、CORS 已连接是不同状态。

Constraints, validated before sending:

- `ip`: dotted decimal IPv4, no leading zeroes, URL or hostname.
  Enabled service requires first octet 1..223 and port 1..65535.
- `mountpoint`: 1..63 ASCII letters/digits/`_`/`-`/`.`; no leading slash.
- `username`: 1..63 printable non-space ASCII characters, no colon.
- `password`: 1..63 printable non-space ASCII characters.
- Disabled configurations allow empty fields but still require valid syntax.

Readback contains IP, port, mountpoint, username, enabled, `credentials_present`,
saved/applied generations and link/freshness flags. **It never contains a password.**
Validation diagnostics do not include supplied values. SDK erases its serialized
secret buffer; the caller owns its string lifetime. Do not log passwords or
password-bearing transport captures.

### Start / 启动

```cpp
const auto current = client.timeSyncCorsConfiguration();
// Display endpoint/account and obtain explicit permission to send live GGA.
prism::RtkStartOptions options;
options.expected_cors_generation = current.saved_generation;
options.allow_gga = user_explicitly_allowed_sending_location;
options.timeout_ms = 20000;
const auto running = client.startRtk(options);
```

GGA reveals live position to the configured service. Default `allow_gga=false`;
enabled CORS requires explicit consent. Bind consent to the account generation
the user reviewed. If it changes, SDK/Agent reject START: query and obtain new
approval instead of silently adopting the changed endpoint.
Saved but unapplied accounts are not started.

RTK-module performs NTRIP login, GGA transmission, RTCM reception and receiver
feeding. Neither SDK opens an Internet CORS connection itself.
With CORS disabled, receiver positioning can start without GGA consent.

### Stop and confirmation / 停止与确认

```cpp
const auto stopped = client.stopRtk();
```

STOP needs no password, GGA consent or configuration query. It commands the
receiver/module to stop RTK and CORS input while preserving timing, rather than
merely hiding SDK output. It does not erase the saved account.

Both methods wait for fresh control readback of the **same command generation**:
2 (`running`) or 4 (`stopped`). ACK/`starting`/`stopping` is not success.
Total query/command/confirmation budget: `timeout_ms` 1..60000, default 20000 ms.
SDK polls at up to 10 Hz; module freshness is reported separately.

Timeout, stale/disconnected readback, superseding commands, receiver error or
configuration change raises an exception. **No save/start/stop is retried automatically.**
A timeout cannot undo a sent command: query `timeSyncRtkStatus()` and configuration
before retrying. 应用须串行调用同一 Client，确认期间不能从另一线程保存账号或关闭连接。

`running` does not guarantee CORS connectivity or FLOAT/FIX. Inspect module
status and `gnssObservations()` GGA/ADRNAV/GST solution type, validity, timestamp
and freshness for actual positioning results.

### Dynamic loading / 动态加载

Direct C++ linking exports the four methods. Explicit DLL loading can resolve
`prism_usb_sdk_get_rtk_module_control_api`, using
`GetRtkModuleControlRuntimeApiFunction` from `<prism/usb/runtime_api.hpp>`.
Request `kRtkModuleControlRuntimeApiVersion` (1); check non-null and `struct_size`.
The table exposes `cors_configuration`, `save_cors_configuration`, `start_rtk`,
`stop_rtk`. Main `RuntimeApi` remains **ABI 18**, unchanged.

<a id="module-versions"></a>

## Module firmware versions / 模块固件版本

Host SDK `prism::Client` 与 RK-local SDK `prism::rklocal::Client` 提供相同的只读接口：

```cpp
const prism::TimeSyncRtkVersions v = client.timeSyncRtkVersions();
if (v.linked && v.application.valid) {
  std::cout << v.application.major << '.' << v.application.minor << '.'
            << v.application.patch << '\n';
}
```

`application` 是 RTK-module 应用固件，`bootloader` 是该模块的引导程序；
它们不是 Sensor Board、Agent 或 GNSS/RTK 芯片的版本。
两者分别包含 `valid`、`major`、`minor`、`patch`、`diagnostic`。
`valid=false` 表示未取得版本，不应显示成 `0.0.0`。
`age_ms` 表示模块链路最近回复的年龄，不是版本读取间隔。

Agent 在 RTK 模式连接模块后低优先级读取版本并缓存，客户端查询不会额外轮询硬件。
无需 GNSS 定位、CORS 连接或启动 RTK；不会切换模式、发送位置或设置账号。
断线时版本无效，重连或检测到模块重启后重新读取。
不支持元数据的模块不影响其他状态查询；持续满载时优先排空 GNSS 数据。

这是新增接口，不改变原有 `timeSyncRtkStatus()`。旧 Agent 不支持新命令时会报错，
界面应单独显示“未提供”，不要将其他设备信息一并隐藏。
查询命令 `0x45`，响应 `0xbc`，空请求；响应版本 1、28 字节，均小端：
`u16 version, u16 size, u32 flags, u32 age_ms, u16 application[4], u16 bootloader[4]`。
flags bit0=连接，bit1=应用版本有效，bit2=引导程序版本有效。
每个版本记录为 major/minor/patch/flags，末字段 bit0 表示诊断应用，引导程序必须为 0。
无效记录全零；连接信息超过 2 秒则不再上报有效版本。

Windows 动态 SDK RuntimeApi 版本为 18；请将头文件与同次构建的库一起更新。
USB 和 RK-local 使用同一解码实现。Web 设备信息页自动刷新，Timesync 页与 Viewer
可用“读取实际状态”刷新只读快照。

<a id="reception-diagnostics"></a>

## GNSS reception diagnostics / 接收诊断

适用于 Agent / Host SDK / RK-local SDK 1.2.0；使用匹配的当前头文件和库。

### 接口与兼容性

**不修改原接口。** `client.gnssTimingStatus()`、`prism::GnssTimingStatus` 的字段、
布局和含义保持原样。时间查询命令仍为 0x3a/0xb6，报文仍为 v5、104 字节；
主 `RuntimeApi` 为 ABI 18；接收诊断使用独立扩展表。

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

### Windows 动态运行库

主运行库导出 `prism_usb_sdk_get_runtime_api(18)`。
新增可选导出 `prism_usb_sdk_get_gnss_reception_api(1)`，返回
`prism::GnssReceptionRuntimeApi`，仅包含版本、大小、`gnss_reception_status` 函数指针。
定义见 `prism/usb/gnss_reception_runtime_api.hpp`。

加载当前主表及独立诊断导出；缺失导出应报告接口不可用。
新 Agent 不支持查询、新库未部署、接收诊断源不可用是不同情况，不应伪装成零接收。
使用头文件+库的常规链接方式时，调用新方法必须链接包含该方法的新库；
二进制兼容仍要求原有编译器、架构和 C++ 运行库条件一致。

Viewer 将新查询与时间查询分别处理。诊断查询失败时显示不可用并停止本次连接的诊断轮询，
重新连接后重试；定位、PPS、授时仍使用各自专用接口，缺失数据不得伪装成零。

### 调用示例

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

### Sensor Board 配套行为

外部输入模式无论 UTC 是否锁定都转发 NMEA/RTCM 及 UART 错误标记。
输出模式仍禁止接收自身输出。20 ms PPS 最低脉宽、RMC 校验、0..800 ms 配对
和 UTC 连贯性检查不放宽。

请使用 Sensor Board 0.4.27 配套固件。接口错误、无接收数据和未锁定 UTC 必须分别报告。
