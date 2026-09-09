# Prism SDK 文档目录

[English](README.md) · [仓库概览](../README.zh-CN.md)

接口参考和使用说明统一维护在本目录。头文件及示例源码仍分别位于
`include/`、`examples/`；`rk-local-sdk/` 保留独立构建入口和测试。

| 内容 | 中文 | English |
| --- | --- | --- |
| 安装及链接方式 | [安装指南](installation.zh-CN.md) | [Installation](installation.md) |
| Host 快速入门 | [使用指南](usage.zh-CN.md) | [Usage](usage.md) |
| Host 完整接口参考 | [开发手册](development-guide.zh-CN.md) | [Development guide](development-guide.md) |
| Host 逐接口代码示例 | [逐接口示例](interface-examples.zh-CN.md) | [Interface examples](interface-examples.md) |
| RK-local C++ 接口、Host 对应关系及限制 | [RK-local 接口说明](rk-local-sdk.zh-CN.md) | [RK-local API](rk-local-sdk.md) |
| RK 本机 ROS 2 Docker（Ubuntu 22.04 / 24.04） | [RK 本机 ROS Docker](rk-local-ros2-docker.zh-CN.md) | [RK-local ROS Docker](rk-local-ros2-docker.md) |
| GNSS、PPS、CORS 与 RTK | [GNSS/RTK](gnss-rtk.zh-CN.md) | [GNSS/RTK](gnss-rtk.md) |
| 编译及运行示例程序 | [示例使用说明](examples.zh-CN.md) | [Examples](examples.md) |
| Release 1.1.0 说明 | [更新说明](update/v1.1.0.zh-CN.md) | [Update notes](update/v1.1.0.md) |

[RK-local 验证记录](rk-local-sdk-testing.md)单独列出已完成测试及尚未验证的实机操作。
同名接口不代表传输方式、时钟来源、队列行为或 ABI 完全一致，替换客户端前请阅读
RK-local 差异表。工作树里的文档更新不代表已经发布新 Release 或 tag。
