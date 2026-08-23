# SDK Usage

This page is the short getting-started path. For every public control,
streaming, parsing, update, and Windows Runtime API entry, see the
[complete SDK development guide](development-guide.md).

## Basic lifecycle on Linux and macOS

The direct `prism::Client` API shown in this section is linkable with the Linux
and macOS runtimes. The Windows package intentionally has no import library;
Windows applications must use Runtime API v5 as shown in
`examples/device_info_time_sync.cpp`.

Include the umbrella header:

```cpp
#include <prism/usb_sdk.hpp>
```

Discover, open, and inspect a device:

```cpp
const auto devices = prism::Client::enumerate();
if (devices.empty()) {
  throw std::runtime_error("no Prism device found");
}

auto client = prism::Client::open(devices.front());
const auto hello = client.hello();
const auto versions = client.deviceVersions();
const auto info = client.deviceInfo();
```

Select an enumerated device by `DeviceInfo::serial_number` when more than one is
connected. USB paths are not stable identities. `product_serial` is populated
only by `client.deviceInfo()` after opening the device.

Before acquisition, check at least:

- `info.usb3_connected` for Camera transport;
- `info.sensor_board_online`;
- `info.sensor_board_time_synced` when synchronized sensor timestamps are
  required;
- `info.camera_present_mask` and `info.imu_present_mask`;
- `info.imu_init_error_mask == 0`;
- `info.sensor_board_error_flags == 0`.

Do not assume that every unit contains two onboard IMUs; use the detected masks
and counts.

## Time synchronization

On Linux and macOS, `synchronizeTimeNtpLike()` measures the device-minus-host
clock offset without changing either clock:

```cpp
const auto measurement = client.synchronizeTimeNtpLike();
```

This measurement is also idle-only: Camera, onboard IMU, and LiDAR transfers
must all be stopped. Runtime API v5 does not expose this measurement-only call
to Windows consumers.

`synchronizeSystemTime()` makes the host wall clock authoritative for the
device and verifies the result:

```cpp
const auto result = client.synchronizeSystemTime();
```

A successful return is already verified. Verification failure is reported as
an exception. Windows performs the same operation through
`RuntimeApi::synchronize_system_time`, as demonstrated by the example.

Before setting time:

1. ensure the host UTC clock is correct;
2. stop Camera, onboard IMU, and LiDAR transfers;
3. call `synchronizeSystemTime()`;
4. check `verified`, the residual offset, and clock-status fields;
5. restart acquisition after the operation completes.

The operation changes RK `CLOCK_REALTIME`, the Ethernet PTP hardware clock, and
the RK RTC; it does not change the host clock. It does not change the
sensor-board GPS/NMEA and PPS time source, and it does not make
`sensor_board_time_synced` true by itself. Never step device time during a
recording.

## Threading and exclusive access

- Only one process may own the device USB interface.
- Use one thread as the `Client` I/O owner.
- Do not call `readFrame()` concurrently from multiple threads.
- Keep Camera decoding, point-cloud rendering, and disk I/O out of the USB
  receive path.
- Stop active streams before closing the client or changing idle-only settings.

## Errors

Public API failures are reported as C++ exceptions. Catch `std::exception` at
application boundaries and include its message in diagnostics. Version
mismatch, USB permissions, an already-open device, unplug events, and attempts
to set time while streaming are expected operational errors and should be shown
clearly to the user.

See the complete development guide above and the authoritative declarations in
the public headers under `include/prism/`.
