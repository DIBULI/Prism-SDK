# RTK-module CORS configuration and control / CORS 配置与 RTK 启停

Agent / Host SDK / RK-local SDK: **1.2.0**. Use matching current headers and
libraries. These wrappers use existing Agent RTK-module commands, without a new
wire protocol or a Sensor Board firmware change.

## Shared C++ API / 两种 SDK 一致的业务接口

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

## Configure / 配置

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

## Start / 启动

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

## Stop and confirmation / 停止与确认

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

## Dynamic loading / 动态加载

Direct C++ linking exports the four methods. Explicit DLL loading can resolve
`prism_usb_sdk_get_rtk_module_control_api`, using
`GetRtkModuleControlRuntimeApiFunction` from `<prism/usb/runtime_api.hpp>`.
Request `kRtkModuleControlRuntimeApiVersion` (1); check non-null and `struct_size`.
The table exposes `cors_configuration`, `save_cors_configuration`, `start_rtk`,
`stop_rtk`. Main `RuntimeApi` remains **ABI 18**, unchanged.
