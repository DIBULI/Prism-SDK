# Prism SDK documentation

[简体中文](README.zh-CN.md) · [Repository overview](../README.md)

Current baseline: **SDK / Agent 1.2.0**, **Sensor Board 0.4.27**. Use matching
headers and libraries. Commands in these guides run from the SDK repository root
unless stated otherwise.

## 1. Start here

- [Install, link, connect and make your first query](getting-started/README.md)
  · [中文](getting-started/README.zh-CN.md)
- Running directly on RK? Start with [RK-local setup and Host differences](reference/rk-local.md).
- [Build and run the examples](examples/README.md) before integrating a feature.

## 2. API reference

| Client | Reference | Transport |
| --- | --- | --- |
| Host `prism::Client` | [Host API](reference/host.md) · [中文](reference/host.zh-CN.md) | USB on Linux, macOS and Windows |
| RK-local `prism::rklocal::Client` | [RK-local API and differences](reference/rk-local.md) · [中文](reference/rk-local.zh-CN.md) | Local Agent socket on RK |

Shared method names do not imply identical clock inputs, capture ownership,
queues or binary ABI. Read the RK-local differences before substituting clients.
The public [headers](../include/prism/) define signatures; feature guides below
collect operational constraints and related examples in one place.

## 3. Feature guides

| Task | One place to read | Language |
| --- | --- | --- |
| Camera exposure, gain, limits and unified auto-exposure | [Camera](guides/camera.md) | EN |
| Receiver results, sky data, CORS accounts, RTK start/stop, versions and diagnostics | [GNSS / RTK](guides/gnss-rtk.md) | EN / 中文 sections |
| UTC validity, manual time-setting and persistent input/output/RTK modes | [Time synchronization](guides/time-sync.md) | EN / 中文 sections |
| LiDAR capture, hardware standby/wake, MID line and XT32 point time | [LiDAR](guides/lidar.md) | EN |
| List and download original datasets over USB or RK-local | [Raw datasets](guides/datasets.md) | 中文 / EN notes |
| Inspect and apply a combined Agent + Sensor Board update | [Firmware update](guides/firmware-update.md) | EN |
| ROS 2 on RK in Ubuntu 22.04 / 24.04 Docker | [ROS 2 on RK](guides/ros2-on-rk.md) · [中文](guides/ros2-on-rk.zh-CN.md) | EN / 中文 |

Frequently used controls:

- [RTK start/stop and CORS configuration](guides/gnss-rtk.md#cors-control)
- [LiDAR standby/wake versus capture start/stop](guides/lidar.md#capture-vs-power)
- [TimeSync mode selection](guides/time-sync.md#port-modes)
- [Four-camera unified automatic exposure](guides/camera.md#unified-automatic-exposure-four-cameras)

## 4. Examples

- [Runnable programs and build commands](examples/README.md) · [中文](examples/README.zh-CN.md)
- [Per-interface code cookbook](examples/interfaces.md) · [中文](examples/interfaces.zh-CN.md)
- [Example source directory](../examples/) — usage comments remain in English.

## 5. Release and package information

- [v1.2.0 release notes](update/v1.2.0.md) · [中文](update/v1.2.0.zh-CN.md)
- [Binary provenance](../ORIGIN.md) · [Package checksums](../SHA256SUMS)

## Documentation layout

`getting-started/` is the first-use path; `reference/` describes clients;
`guides/` groups features; `examples/` explains runnable code; `update/` keeps
release history. Add new details to the relevant feature guide rather than
creating another top-level one-feature document. Keep both indexes and all links
in sync when moving a page.

Run `python3 scripts/check_docs.py` to validate local links, section anchors and
index coverage. It also runs as part of `scripts/test_all_examples.py --verify-only`.
