# Prism SDK 文档目录

[English](README.md) · [仓库概览](../README.zh-CN.md)

当前配套版本：**SDK / Agent 1.2.0**、**Sensor Board 0.4.27**。头文件与库须配套使用。
除非另有说明，文档中的命令均在 SDK 仓库根目录执行。

## 1. 快速开始

- [安装、链接、连接设备与首次读取](getting-started/README.zh-CN.md)
  · [English](getting-started/README.md)
- 在 RK 内运行？先看 [RK-local 安装及与 Host 的差异](reference/rk-local.zh-CN.md)。
- 接入具体功能前，先[编译并运行示例](examples/README.zh-CN.md)。

## 2. 接口参考

| 客户端 | 文档 | 连接方式 |
| --- | --- | --- |
| Host `prism::Client` | [Host 接口](reference/host.zh-CN.md) · [English](reference/host.md) | Linux、macOS、Windows 的 USB 连接 |
| RK-local `prism::rklocal::Client` | [RK-local 接口与差异](reference/rk-local.zh-CN.md) · [English](reference/rk-local.md) | RK 内部 Agent socket |

同名方法不代表时钟输入、采集所有权、队列和二进制 ABI 完全相同。替换客户端前请阅读
RK-local 差异表。接口签名以[公共头文件](../include/prism/)为准；功能约束和相关示例统一放在下面的专题页。

## 3. 功能指南

| 要做什么 | 集中说明 | 语言 |
| --- | --- | --- |
| 相机曝光、增益、限制、四路统一自动曝光 | [相机](guides/camera.md) | 英文 |
| 定位结果、天空图、CORS 账号、RTK 启停、版本与接收诊断 | [GNSS / RTK](guides/gnss-rtk.md#receiver-results-zh) | 中英文分节 |
| UTC 有效性、手动校时、输入/输出/RTK 模式与持久化 | [时间同步](guides/time-sync.md) | 中英文分节 |
| 雷达采集、待机/唤醒、MID line、XT32 逐点时间 | [雷达](guides/lidar.md) | 英文 |
| 单路缺失与部分相机帧组 | [独立数据流](guides/stream-resilience.md) | 英文 |
| 通过 USB 或 RK-local 浏览、下载原始数据集 | [原始数据集](guides/datasets.md) | 中文，含英文部署说明 |
| 检查及应用 Agent + Sensor Board 联合升级包 | [固件升级](guides/firmware-update.md) | 英文 |

## 4. 示例

- [可运行程序与编译命令](examples/README.zh-CN.md) · [English](examples/README.md)
- [逐接口代码示例](examples/interfaces.zh-CN.md) · [English](examples/interfaces.md)
- [示例源码目录](../examples/)：使用说明继续以英文写在源码注释中。

## 5. 版本与发布信息

- [v1.2.0 更新说明](update/v1.2.0.zh-CN.md) · [English](update/v1.2.0.md)
- [包兼容说明](../ORIGIN.md) · [文件校验清单](../SHA256SUMS)

## 文档维护约定

`getting-started/` 放首次使用流程，`reference/` 放客户端参考，`guides/` 按功能归档，
`examples/` 放示例说明，`update/` 保留版本记录。后续新增功能优先补充对应专题页，
不再散落到 docs 根目录；移动文档时同步更新中英文目录和相关链接。

运行 `python3 scripts/check_docs.py` 校验本地链接、章节锚点和目录覆盖；
`scripts/test_all_examples.py --verify-only` 也会执行这项检查。
