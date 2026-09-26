# RTK-module 版本查询

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
