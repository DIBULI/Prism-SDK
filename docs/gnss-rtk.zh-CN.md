# GNSS 与接收机原生 RTK

[English](gnss-rtk.md) · [接口手册](development-guide.zh-CN.md)

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

详见 [CORS/RTK 控制](rtk-module-control.md)、[TimeSync](timesync-port.md)、
[接收诊断](gnss-reception-status.md)和[只读示例](../examples/gnss_rtk_status.cpp)。
