# RK-local C++ control/API validation — 2026-09-07

RK-local C++ update validation. SDK/Agent remain 1.1.0.
Host runtime binaries and Sensor Board firmware are unchanged. A temporary Agent
lock-isolation fix was deployed for physical testing; see below.

Final physical query/capture/stop/reconnect validation: **19/19 passed**.
This does not imply every write/upgrade or live GNSS/CORS scenario was exercised.

## Source and artifact

- Source baseline: Agent e47627fab53946c854bbdb0f83fee9a0a209d98e plus the
  uncommitted RK-local C++ migration, shared Host controls, raw transport and tests.
- Source: /work/projects/Prism-agent/prism-rklocal-sdk.
- Packaged archive: runtime/linux-arm64/libprism_rklocal_sdk.a.
- SHA-256: 66032fa4ed8f95ad39963cab0fbcd3613828f3045f1fb483d7d2ebec3d4d9dac.
- Built in the existing prism-ros-adapter:noetic ARM64 Ubuntu 20.04 container,
  GCC 9.4.0, original source directory mounted in place.
- Final build directory: prism-rklocal-sdk/build/arm64-ubuntu20.
- Static OpenSSL 1.1.1f-1ubuntu2.24 and miniz included for ZIP/SHA validation.
- The control/upgrade test executable requires maximum GLIBC 2.17 / GLIBCXX 3.4.21.
  It has no dynamic libcrypto/libssl/libusb dependency. System C/C++/pthread/dl remain required.
- The earlier GCC 10 cross build was not selected for distribution because its
  new control/upgrade test depended on newer GLIBC symbols.

## Completed checks

| Check | Result |
| --- | --- |
| Linux x64: API parity, new controls, C transport mock, C++ capture/GNSS mock | 4/4 passed |
| ASan + UBSan, same four source tests | 4/4 passed |
| GCC 10 ARM64 cross build and QEMU execution | 4/4 passed |
| Final Ubuntu 20.04 ARM64/GCC 9 build and container execution (QEMU) | 4/4 passed |
| Packaged ARM64 library: API test, parity test, two example help commands | 4/4 passed |
| Modified shared Host source: static library/examples and existing codec/runtime tests | 11/11 passed |

API parity covers parameters and return types of 51 shared interfaces, including
both sendRtkCorrections overloads. It deliberately excludes transport-specific
factories and does not claim ABI, wrapper-class or exact exception equivalence.

Mock checks include configuration persistence acknowledgements, exposure/limits,
LiDAR controls/network, Wi-Fi, keepalive, raw rover events, bounded queues/drop
counts, RTCM chunking, command timeout/late-reply rejection, Agent errors,
GPS-lock time-write rejection, explicit UTC verification against a mock clock,
joint ZIP manifest/SHA validation and asynchronous sensor-board OTA failure
progress. Existing tests cover four-camera JPEG ownership, IMU, raw/smoothed
RTK, GNSS/PPS fields, move/close semantics and capture cleanup.


## Physical RK3576 validation

Test date: 2026-09-07. Device runs Debian 12 ARM64, image 2026.09.06.2,
Agent 1.1.0 and Sensor Board 0.4.26. Only IMU0 is installed. The published
ARM64 archive above was linked into the consumer examples and opt-in hardware
test. No camera/IMU dataset or JPEG files were saved.

The original Agent could accept the local socket but HELLO timed out. Its USB
writer was blocked inside FunctionFS while holding a write mutex shared with
RK-local. A temporary Agent build separates USB/local write locks (including
TIME_SYNC replies). The server currently accepts only one local protocol client;
the lock follows that connection until its writer is joined.

- Temporary Agent SHA-256:
  0439344729dc1a481a5ebc9145a04093c6f8aa93789ac491fdc45b924ede6187.
- Installed original /usr/sbin/prism-agent was not overwritten; a /run systemd
  override selects the temporary binary. Reboot removes the override.
- Sensor Board firmware, persistent configuration and device time were not written.
- Agent regression suite: 22/22 passed, including a stalled-USB/local-write test.
- 16 query interfaces plus raw heartbeat and close/reopen: 18/18 passed.
- A real Host SDK 1.1.0 query was then closed, leaving the USB writer blocked in
  FunctionFS. RK-local still completed GNSS queries and the same 18 checks.
- GNSS was queried at a 100 ms cadence. The actual input had no NMEA/PPS and no
  CORS data; unavailable GPS/RTK was correctly reported. This does not validate
  live GPS, RTCM2/3 decoding, CORS throughput or RTK position accuracy.
- First 60-second capture: 1,796 four-camera sets, 47,839 IMU0 samples;
  sensor timestamp rate 29.985 FPS / 797.318 Hz (nominal IMU setting 800 Hz).
  All four 1280x1024 JPEG markers and metadata checked; no image sequence gaps.
- An early diagnostic reported one IMU sequence gap. This was a test error:
  the wire carries the Sensor Board's **16-bit** sequence in a 32-bit field.
  The test now checks modulo 65536, not modulo 2^32.
- DeviceInfo's IMU receiving mask describes a freshness window, not the STOP
  acknowledgement. The test now waits up to five seconds for it to expire.
- One 20-second run observed a real source timestamp reversal at sample 285:
  sequence 38022 -> 38023, 15942001318 -> 15942000625 us (-693 us), flags 0x8b
  on the latter sample. The Sensor Board decoder and both SDK paths copy this
  timestamp without smoothing/re-timing. This observation must not be hidden
  by changing SDK timestamps or silently dropping samples.
- A subsequent 60-second run identified the transition precisely: sample 349,
  sequence 5490 -> 5491, 16166000806 -> 16166000625 us (-181 us), flags
  **0x08 (not timestamp-synced) -> 0x8b (timestamp-synced)**. This is the first
  Sensor Board FSYNC anchor, not a reversal within a synchronized interval.
  The test now reports these unsynchronized reanchors separately and fails
  any non-increasing timestamp between two synchronized samples. Compile-time
  regression checks cover sequence wrap and both kinds of timestamp transition.

- Final 60.000-second run: **19/19 checks passed**; 1,792 four-camera sets
  (7,168 JPEGs), 47,838 IMU0 samples including 47,518 synchronized samples.
  Timestamp-derived rates: **29.985 FPS / 797.294 Hz**. No frame/IMU sequence
  gaps, invalid JPEGs/metadata, frame reversals, synchronized IMU reversals or
  raw-queue drops. STOP became idle and reconnect succeeded; configuration
  remained 10 FPS / 800 Hz / quality 88 / GNSS 460800, generation 1.
- One explicit startup reanchor was retained: sample 320, flags 0x08 -> 0x8b,
  16346000890 -> 16346000625 us (-265 us). Initial arrival-rate measurements
  include camera pipeline warm-up and are not steady-state source FPS.
- Logs are retained in the server's SDK build directory:
  rklocal-hardware-qualified-20260907.log and rklocal-usb-isolation-20260907.log.

The opt-in test is compiled but is **not registered in CTest**:

```sh
# Read-only query, event and reconnect checks:
build/rklocal/tests/prism-rklocal-hardware-test /run/prism/stream.sock 0
# Explicit 60-second four-camera + IMU0 capture, with continuity checks:
build/rklocal/tests/prism-rklocal-hardware-test /run/prism/stream.sock 60
```

Capture refuses a device already reporting active camera/IMU data. It uses
runtime 30 FPS and checks that persistent configuration is unchanged afterward.
It fails synchronized timestamp reversals, sequence gaps, corrupt image/metadata,
missing synchronized samples, failure to stop, or persistent configuration changes.
Initial unsynchronized timestamp transitions are reported explicitly; no data is dropped.

## Not validated in this update

No real device clock/configuration changes, Sensor Board flashing or live CORS
injection were performed. Agent restart was a manual deployment, not an SDK upgrade. Full successful firmware replacement,
sensor-board restart, Agent restart/reconnect and rollback remain hardware
validation items; mock OTA failure testing is not proof of those operations.

Physical tests used Debian 12 on RK3576, not Ubuntu 20.04/22.04/24.04 device certification. Full GitHub/macOS/Windows release jobs were
not rerun as part of the local hardware validation. Release/CI status is tracked
separately on GitHub; the measurements above describe the exact tested files.
See [RK-local API guide](rk-local-sdk.md#host-api-alignment-and-explicit-differences)
for explicit API/transport/clock/queue/linking differences.
