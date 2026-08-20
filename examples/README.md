# Prism SDK examples

[简体中文](README.zh-CN.md)

These examples are intentionally small and safe to run. None of them writes
persistent device configuration, changes exposure, upgrades firmware, or
changes the Wi-Fi/LiDAR network configuration. Time synchronization is also
opt-in.

## Build

Build all examples from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or build the examples directory as a standalone CMake project:

```bash
cmake -S examples -B build-example -DCMAKE_BUILD_TYPE=Release
cmake --build build-example --config Release
```

Only one process can own a Prism USB device at a time. Close Prism Viewer and
other SDK programs before running an example.

## `prism-device-info-time-sync`

Platforms: Linux x86-64, macOS arm64, and Windows x64.

This cross-platform example demonstrates the supported device lifecycle:

1. enumerate Prism USB devices;
2. open the first device;
3. read version and health information;
4. optionally synchronize device time to the host;
5. close the device cleanly.

Read device information only:

```bash
./build/examples/prism-device-info-time-sync
```

Set and verify device time:

```bash
./build/examples/prism-device-info-time-sync --sync-time
```

`--sync-time` is an administrative operation. The host clock must be correct,
and all Camera, IMU, and LiDAR streams must be stopped. On Windows, this
example also demonstrates loading `prism_usb_sdk.dll` and validating Runtime
API v4 before calling it.

## `prism-camera-imu-capture`

Platforms: Linux x86-64 and macOS arm64.

This finite-duration example starts the aggregate Camera/board-IMU session,
uses one USB receive loop, dispatches IMU samples through `ImuStream`, parses
Camera chunks and metadata, and stops both streams explicitly. It requires
strictly contiguous JPEG chunk offsets and sends `sendVideoAck()` after all
Camera images and the matching metadata have arrived. A corrupt or incomplete
frame is retired and acknowledged so it cannot exhaust device flow-control
credit, but invalid metadata is never used for timestamps or exposure. JPEG
bytes are not saved.

Use the persistent Camera FPS and IMU rate for ten seconds:

```bash
./build/examples/prism-camera-imu-capture
```

Temporarily request 20 FPS and 500 Hz for thirty seconds:

```bash
./build/examples/prism-camera-imu-capture \
  --seconds 30 --fps 20 --imu-rate 500
```

`--fps 0` and `--imu-rate 0` select the persistent device values. Camera FPS
may be any integer from 1 through 30; IMU rate may be 500 or 1000 Hz. These
start parameters are not saved as persistent configuration.

The example uses `parseVideoChunkView()` only while its source `Frame` is
alive. Copy the JPEG bytes before passing them to another thread or retaining
them after the receive-loop iteration.

Run the deterministic frame-tracker test without a device:

```bash
./build/examples/prism-camera-imu-capture --self-test
```

The test covers valid and invalid metadata, contiguous multi-chunk images,
duplicate chunks, missing byte ranges, and retirement of an incomplete older
frame when the next frame ID appears. CTest runs it as
`prism-camera-frame-tracker-self-test`.

## `prism-lidar-capture`

Platforms: Linux x86-64 and macOS arm64.

This finite-duration example uses `LidarStream` to receive point batches and
LiDAR IMU samples in the same USB receive loop. The LiDAR model is mandatory
because Mid360 and Mid360S use different configurations and the SDK does not
guess the model.

Capture a Mid360 for ten seconds:

```bash
./build/examples/prism-lidar-capture --model mid360
```

Capture a Mid360S for thirty seconds:

```bash
./build/examples/prism-lidar-capture \
  --model mid360s --seconds 30
```

Configure and probe the LiDAR network while all streams are idle before using
this example. It reports synchronized timestamp counts separately; consumers
that require UTC must reject samples whose `timestamp_synced` field is false.

## Exit status

All examples return zero on success and one on an argument, connection, or SDK
error. Streaming examples return three if the requested interval completes
without a complete Camera frame set or LiDAR point batch, respectively.

For examples of every public SDK interface, including persistent settings,
exposure control, Wi-Fi, LiDAR network management, raw frame parsing, and
system upgrades, see the complete [SDK development guide](../docs/development-guide.md).
