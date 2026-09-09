# Prism SDK documentation

[简体中文](README.zh-CN.md) · [Repository overview](../README.md)

Interface reference and usage documentation is maintained in this directory.
Headers and example sources remain in `include/` and `examples/`;
`rk-local-sdk/` retains its standalone build entry and tests.

| Topic | English | 简体中文 |
| --- | --- | --- |
| Installation and linking | [Installation](installation.md) | [安装指南](installation.zh-CN.md) |
| Host quick start | [Usage](usage.md) | [使用指南](usage.zh-CN.md) |
| Host complete API reference | [Development guide](development-guide.md) | [开发手册](development-guide.zh-CN.md) |
| Host per-interface snippets | [Interface examples](interface-examples.md) | [逐接口示例](interface-examples.zh-CN.md) |
| RK-local C++ API, Host differences and limitations | [RK-local API](rk-local-sdk.md) | [RK-local 接口说明](rk-local-sdk.zh-CN.md) |
| ROS 2 Docker on RK (Ubuntu 22.04 / 24.04) | [RK-local ROS Docker](rk-local-ros2-docker.md) | [RK 本机 ROS Docker](rk-local-ros2-docker.zh-CN.md) |
| GNSS, PPS, CORS and RTK | [GNSS/RTK](gnss-rtk.md) | [GNSS/RTK](gnss-rtk.zh-CN.md) |
| Build and run example programs | [Examples](examples.md) | [示例使用说明](examples.zh-CN.md) |
| Release 1.1.0 notes | [Update notes](update/v1.1.0.md) | [更新说明](update/v1.1.0.zh-CN.md) |

See the [RK-local validation record](rk-local-sdk-testing.md) for completed checks
and unverified hardware operations. Shared method names do not imply identical
transport, clock source, queue behavior or ABI; consult the RK-local differences
before substituting clients. Working-tree documentation does not mean a new
Release or tag has been published.
