# Ethernet image and IMU capture (SDK 1.3.0)

[Examples](../README.md) · [中文](README.zh-CN.md)

`network_camera_imu_capture.cpp` opens a network Client and receives JPEG images and onboard IMU samples in one loop. Supported: Linux x64/arm64 and macOS arm64. The Windows package uses RuntimeApi and does not build this direct Client example.

Build from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target prism-network-camera-imu-capture --parallel
```

Use a compatible 1.3.0 device gateway on a trusted LAN. Stop any existing Viewer/Web capture first. Replace the numeric IP below; the output directory must not exist and its parent must exist. Read the Web admin password without terminal echo in Bash:

```bash
read -r -s -p 'Device Web password: ' PRISM_PASSWORD; echo
printf '%s\n' "$PRISM_PASSWORD" | ./build/examples/prism-network-camera-imu-capture \
  --host 192.168.1.100 --output capture-001 --seconds 10 --fps 20
unset PRISM_PASSWORD
```

The password is read from stdin, never a command-line argument or output file. Transport is plaintext TCP; use only trusted networks. Default port: 7443 (`--port` overrides it). `--host` takes numeric IPv4/IPv6, without DNS. Ethernet is capped at 20 FPS, Wi-Fi/unknown links at 10 FPS; native profile limits can be lower. The program reports negotiated and active FPS. Defaults `--fps 0 --imu-rate 0` use device settings; `--imu-rate 800` explicitly requests the onboard rate. Network camera caps do not reduce IMU rates.

Outputs:

- `cam<camera>_<frame>.jpg`: assembled JPEG images; missing cameras remain absent.
- `frames.csv`: camera/frame IDs, timestamp, dimensions, metadata validity, trigger time and filename.
- `imu.csv`: sensor/sample IDs, timestamp, synchronization flag, raw flags, FSYNC, gap flag, acceleration (mg), angular velocity (mdps), temperature (milli-degrees Celsius). Multiply acceleration by 0.00980665 for m/s² and angular velocity by π/180000 for rad/s.

IMU `timestamp_us` is synchronized UTC only when `timestamp_synced=1`; otherwise it is sensor-board local time. Image timestamps and metadata validity are preserved without claiming UTC lock or matching IMU samples by image frame ID. This example requests legacy JPEG; see [network and RAW12](../../docs/guides/network-raw.md) for native lossless capture APIs.

Feed each frame to `ImuStream::handleFrame()`, then video chunks/metadata to `CameraFrameAssembler`. ACK completed and retired frame sets exactly once, preserving complete images in partial sets. Expire assemblies even when no packets arrive. Read timeouts continue; disconnects and file errors fail. Ctrl+C or the duration limit attempts STOP and closes files; unfinished sets at shutdown are not saved. Exit 0 requires both images and IMU, 3 means at least one stream delivered no data, and 1 means a parameter/runtime error.

Synchronous disk writes keep this example simple; slow storage can cause dropped frames. For sustained throughput use a bounded writer queue and monitor frame/sample gaps. This is not a lossless recorder guarantee.

For a query-only connection, use `prism-network-connect numeric-IP [port]` with the password on stdin. Both programs support offline `--help`.
