# ROS 2 Docker on RK with the RK-local SDK

[简体中文](rk-local-ros2-docker.zh-CN.md) · [Documentation index](README.md) · [RK-local API](rk-local-sdk.md)

Run Prism ROS Adapter on RK3576 through the Agent's Unix socket
`/run/prism/stream.sock`. The container uses the RK-local SDK, not USB, and
does not start another Agent or replace the host operating system.

| Container OS | ROS 2 | Architecture | Local Docker image |
| --- | --- | --- | --- |
| Ubuntu 22.04 | Humble | ARM64 | `prism-ros-adapter:humble-rklocal` |
| Ubuntu 24.04 | Jazzy | ARM64 | `prism-ros-adapter:jazzy-rklocal` |

## Availability and requirements

This is a documentation-only update. The RK-local ROS adapter implementation,
Dockerfile, build/run helpers and probes are currently in the development
working tree of **Prism-ROS-adapter**, not published with this SDK commit or
the SDK `v1.1.0` tag. A fresh GitHub checkout does not yet include those adapter
changes. The image names above are locally built images, not Docker Hub releases.

- Agent **1.1.0** must be running with its RK-local socket enabled. The build
  uses SDK **1.1.0**, commit `c5e62d0685deeba85afb9eed34ea3d3ac3c36063`.
- Docker must be installed on RK, with enough free root-filesystem space to
  load and unpack the images. Check `df -h /` and `test -S /run/prism/stream.sock`.
- Close Viewer/USB capture and other RK-local capture clients. Run only one
  capture container at a time; this does not change Agent connection-lock behavior.
- Use trusted images: access to the socket grants device-control access, and
  host networking shares the RK network namespace.

## Load an offline image

Obtain the ARM64 offline packages produced by the build server:

- `prism-ros2-humble-rklocal-arm64.tar.gz` and its `.sha256` file.
- `prism-ros2-jazzy-rklocal-arm64.tar.gz` and its `.sha256` file.

On RK, from the directory containing the packages:

```bash
sha256sum -c prism-ros2-humble-rklocal-arm64.tar.gz.sha256
sudo docker load -i prism-ros2-humble-rklocal-arm64.tar.gz
sha256sum -c prism-ros2-jazzy-rklocal-arm64.tar.gz.sha256
sudo docker load -i prism-ros2-jazzy-rklocal-arm64.tar.gz
sudo docker images
```

No ROS installation is needed on the host. Without GNSS, the RK clock may
intentionally be near the Unix epoch. Do not change it merely to make HTTPS
downloads work; transfer server-built packages instead.

## Start on RK

On a device where the launcher has already been installed, choose one:

```bash
sudo prism-ros2-docker 22.04
# Stop with Ctrl-C before starting the other distribution.
sudo prism-ros2-docker 24.04
```

The launcher is `/usr/local/bin/prism-ros2-docker`; it is an optional installed
tool, not automatically included in a fresh SDK checkout or flash image.
Without it, use the equivalent command below (replace `humble` with `jazzy`
for Ubuntu 24.04):

```bash
sudo docker run --pull never --rm --init --network host --ipc host \
  --cap-drop ALL --security-opt no-new-privileges \
  --mount type=bind,src=/run/prism,dst=/run/prism,readonly \
  -e ROS_DOMAIN_ID=0 \
  prism-ros-adapter:humble-rklocal
```

Default acquisition includes cameras, detected board IMU and navigation;
LiDAR is disabled by default. One detected IMU is sufficient. Camera and IMU
rates follow device settings (`camera_fps=0`, `imu_rate_hz=0`); JPEGs are not
decoded or re-encoded. No image data is recorded by this launch command.

For GNSS/RTK queries without starting capture, append to the Docker command:

```bash
ros2 run prism_ros_driver prism_ros_driver_node --ros-args \
  -p camera_enabled:=false -p board_imu_enabled:=false
```

Other ROS clients must use the same `ROS_DOMAIN_ID`. A CLI shell does not open
another SDK capture session:

```bash
sudo docker run --pull never --rm -it --network host --ipc host \
  -e ROS_DOMAIN_ID=0 prism-ros-adapter:humble-rklocal bash
ros2 topic list
ros2 service call /prism/gnss/get_timing prism_ros_msgs/srv/GetGnssTiming '{}'
```

## Minimal RK kernel: Docker bridge/NAT failure

If Docker fails with a missing MASQUERADE/NAT rule, the kernel may lack support
for Docker's default bridge. A dedicated host-network-only ROS Docker host can
use the following `/etc/docker/daemon.json`. Back up and review any existing
configuration first; do not blindly overwrite it:

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

After saving the configuration, when no containers are running:

```bash
sudo systemctl reset-failed docker.service docker.socket
sudo systemctl restart docker
sudo docker info
```

This mode requires `--network host` or `--network none`; bridge/NAT networking
and `-p` port publishing are unavailable. It does not stop or reconfigure
`prism-ptp4l.service` or `prism-phc2sys.service`.

## Interfaces and limitations

Topic names, messages and CORS raw-RTCM input follow
[Prism-ROS-adapter's interface documentation](https://github.com/DIBULI/Prism-ROS-adapter/blob/master/README.md#published-topics).
SDK-level details are in the [RK-local API](rk-local-sdk.md) and
[GNSS/RTK guide](gnss-rtk.md).

- SDK/Agent accept raw RTCM data; the CORS/NTRIP client handles server login.
- `device_serial` is USB-only; `rklocal_socket` selects a Unix socket, not TCP.
- Camera and board IMU share acquisition; stopping either stops both.
- RK-local automatically ACKs assembled video. The adapter does not ACK twice.
  Raw-event queue overflow stops acquisition with an error.
- The container shares the RK clock. `/prism/system/sync_time` with
  `confirm=true` fails on RK-local instead of setting the clock to itself.
  Sensor-board time remains authoritative; PTP stays on the host.
- GNSS status queries succeeding does not prove GPS/PPS lock or an RTK fix.
  LiDAR requires correct device-side model/network configuration and hardware.
- IMU uses best-effort QoS, not a lossless-recording guarantee. Under camera
  load, Python subscriber rates can be lower than driver input rates. Compare
  `/diagnostics` counters with a lightweight C++ subscriber; timestamp gaps
  alone are not proof of dropped samples because clock adjustments can affect them.
- ROS 1 remains USB-only. No firmware-upgrade services are added.

## Building from the development working tree

These commands require the **unpublished RK-local changes** in the existing
Prism-ROS-adapter working tree, not this SDK repository:

```bash
bash scripts/docker_build_rklocal.sh 22.04
bash scripts/docker_build_rklocal.sh 24.04
```

Build on the server with native ARM64 or registered AArch64 QEMU/binfmt and
BuildKit named-context support. The build links the SDK's ARM64 static archive;
it does not compile Agent sources. Ubuntu/ROS APT packages use signed Tsinghua
mirrors. QEMU build success is not a substitute for native RK runtime validation.

Keep raw test logs, capture data and local test reports outside Git history.
