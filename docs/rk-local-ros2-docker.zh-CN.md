# RK 本机通过 RK-local SDK 使用 ROS 2 Docker

[English](rk-local-ros2-docker.md) · [文档目录](README.zh-CN.md) · [RK-local 接口](rk-local-sdk.zh-CN.md)

ROS 节点运行在 RK3576 本机的 Docker 容器中，通过
`/run/prism/stream.sock` 连接现有 Agent，使用 RK-local SDK，不走 USB。
容器不会启动第二个 Agent，也不会替换 RK 的宿主系统。

| 容器系统 | ROS 2 | 架构 | 本地镜像名称 |
| --- | --- | --- | --- |
| Ubuntu 22.04 | Humble | ARM64 | `prism-ros-adapter:humble-rklocal` |
| Ubuntu 24.04 | Jazzy | ARM64 | `prism-ros-adapter:jazzy-rklocal` |

## 发布状态与前提

本次只发布文档。RK-local ROS 适配代码、Dockerfile、构建/启动脚本和测试探针
目前仍位于 **Prism-ROS-adapter 的开发工作树**，没有随本次 SDK 文档提交或
SDK `v1.1.0` 标签发布。直接克隆 GitHub 仓库还不能获取这些适配改动。
上表名称是本地构建产物，不是已发布到 Docker Hub 的镜像。

- RK 运行 Agent **1.1.0**，并启用 RK-local socket。构建使用 SDK **1.1.0**，
  固定提交 `c5e62d0685deeba85afb9eed34ea3d3ac3c36063`。
- RK 已安装 Docker；根分区应有足够空间用于加载和解包镜像。
  使用 `df -h /`、`test -S /run/prism/stream.sock` 检查。
- 关闭 Viewer/USB 采集和其他 RK-local 采集客户端；一次只运行一个采集容器。
  本功能不修改 Agent 不同连接之间的锁与采集管理行为。
- 只运行可信镜像：访问 socket 即具有设备控制能力；host 网络共享 RK 网络命名空间。

## 加载离线镜像

从构建服务器获取以下 ARM64 包及对应的 `.sha256` 校验文件：

- `prism-ros2-humble-rklocal-arm64.tar.gz`
- `prism-ros2-jazzy-rklocal-arm64.tar.gz`

复制到 RK 后，在文件所在目录执行：

```bash
sha256sum -c prism-ros2-humble-rklocal-arm64.tar.gz.sha256
sudo docker load -i prism-ros2-humble-rklocal-arm64.tar.gz
sha256sum -c prism-ros2-jazzy-rklocal-arm64.tar.gz.sha256
sudo docker load -i prism-ros2-jazzy-rklocal-arm64.tar.gz
sudo docker images
```

宿主机不需要安装 ROS。没有 GNSS 时，RK 时间可能有意保持在 Unix 启动纪元附近。
不要为了通过 HTTPS 证书时间校验而随意设置 RK 时间，应从服务器传输离线包。

## 在 RK 上启动

已经安装启动工具的设备，可以二选一：

```bash
sudo prism-ros2-docker 22.04
# Ctrl-C 停止后，再运行另一种版本。
sudo prism-ros2-docker 24.04
```

工具路径是 `/usr/local/bin/prism-ros2-docker`，属于额外安装的启动工具，
不是新克隆 SDK 仓库或重新烧录镜像后一定存在的命令。
没有该工具时，可以直接执行下列等价命令；Ubuntu 24.04 将 `humble` 改为 `jazzy`：

```bash
sudo docker run --pull never --rm --init --network host --ipc host \
  --cap-drop ALL --security-opt no-new-privileges \
  --mount type=bind,src=/run/prism,dst=/run/prism,readonly \
  -e ROS_DOMAIN_ID=0 \
  prism-ros-adapter:humble-rklocal
```

默认启用相机、检测到的板载 IMU 和导航状态；默认关闭 LiDAR。
只有一个 IMU 也可采集。相机和 IMU 频率跟随设备设置
（`camera_fps=0`、`imu_rate_hz=0`）。JPEG 直接转发，不进行解码/重编码；
上述启动命令不会把图像录制到磁盘。

只查询 GNSS/RTK、不启动采集时，在上述 Docker 命令末尾追加：

```bash
ros2 run prism_ros_driver prism_ros_driver_node --ros-args \
  -p camera_enabled:=false -p board_imu_enabled:=false
```

其他 ROS 客户端应使用相同的 `ROS_DOMAIN_ID`。另开一个命令行容器进行查询，
不会再建立一个 SDK 采集会话：

```bash
sudo docker run --pull never --rm -it --network host --ipc host \
  -e ROS_DOMAIN_ID=0 prism-ros-adapter:humble-rklocal bash
ros2 topic list
ros2 service call /prism/gnss/get_timing prism_ros_msgs/srv/GetGnssTiming '{}'
```

## 精简 RK 内核的 Docker 网桥/NAT 问题

如果 Docker 启动提示缺少 MASQUERADE/NAT 规则，可能是内核未包含默认网桥需要的功能。
专门使用 host 网络运行 ROS 的 Docker 主机，可以采用以下 `/etc/docker/daemon.json`。
先备份并检查已有配置，不要直接覆盖已有文件：

```json
{
  "bridge": "none",
  "iptables": false,
  "ip6tables": false,
  "ip-forward": false,
  "ip-masq": false,
  "userland-proxy": false
}
```

保存后，在没有容器运行时执行：

```bash
sudo systemctl reset-failed docker.service docker.socket
sudo systemctl restart docker
sudo docker info
```

这种配置要求使用 `--network host` 或 `--network none`，不支持网桥/NAT 和 `-p` 端口映射。
不会停止或修改 `prism-ptp4l.service`、`prism-phc2sys.service`。

## 接口与限制

话题名、消息和 CORS 原始 RTCM 输入方式参考
[Prism-ROS-adapter 接口说明](https://github.com/DIBULI/Prism-ROS-adapter/blob/master/README.md#published-topics)。
SDK 接口详见 [RK-local](rk-local-sdk.zh-CN.md) 和 [GNSS/RTK](gnss-rtk.zh-CN.md)。

- SDK/Agent 接收原始 RTCM；CORS/NTRIP 登录由上层客户端负责。
- `device_serial` 只用于 USB；`rklocal_socket` 是 Unix socket 路径，不是 TCP 地址。
- 相机和板载 IMU 共用采集会话，停止任意一个都会停止二者。
- RK-local 自动 ACK 完整视频帧，ROS 不重复 ACK。原始事件队列溢出时会报错停止采集。
- 容器共享 RK 时钟，因此 RK-local 的 `/prism/system/sync_time` 在 `confirm=true`
  时返回失败，不会把 RK 时间再设置给自己。传感板仍是时间源，PTP 运行在宿主机。
- GNSS 查询成功不代表 GPS/PPS 已锁定或 RTK 已出解。LiDAR 仍需正确的型号、网络配置和硬件连接。
- IMU 使用 best-effort QoS，不保证零丢包。四路相机负载下，Python 订阅计数可能低于驱动
  实际收样率；应结合 `/diagnostics` 和轻量 C++ 订阅器判断。时间戳间隔也可能受校时影响，
  不能仅凭时间戳跳变断言丢样。
- ROS 1 仍只支持 USB；不提供固件升级 service。

## 从开发工作树构建

以下命令要求已有 **尚未发布的 RK-local 适配改动**，应在 Prism-ROS-adapter 的原开发目录
执行，而不是在本 SDK 仓库或直接新克隆的 ROS adapter 仓库执行：

```bash
bash scripts/docker_build_rklocal.sh 22.04
bash scripts/docker_build_rklocal.sh 24.04
```

在服务器上构建，使用原生 ARM64 或已注册 AArch64 QEMU/binfmt 的环境，以及支持命名
构建上下文的 BuildKit。直接链接 SDK 的 ARM64 静态库，不编译 Agent 源码。
Ubuntu/ROS APT 使用清华镜像源并保留签名校验；QEMU 构建成功不能代替 RK 原生运行验证。

原始测试日志、采集数据及本地验证报告不提交到 Git 仓库。
