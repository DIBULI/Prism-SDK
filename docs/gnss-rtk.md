# SDK 1.1.0: GNSS, CORS and navigation

Use matching 1.1.0 headers/libraries and Agent 1.1.0. Windows uses Runtime API
12, an MSVC C++ ABI table, not a compiler-neutral C ABI. RK-local is a C++17
interface to `/run/prism/stream.sock` on the device, not USB or direct UART access.

| Operation | Host `Client` | Windows `RuntimeApi` | RK-local C++ Client |
| --- | --- | --- | --- |
| GNSS/PPS snapshot | `gnssTimingStatus()` | `gnss_timing_status` | `gnssTimingStatus` |
| Correction transport status | `rtkCorrectionStatus()` | `rtk_correction_status` | `rtkCorrectionStatus` |
| Submit corrections | `beginRtkCorrections`, `sendRtkCorrections`, `endRtkCorrections` | `begin_rtk_corrections`, `send_rtk_corrections`, `end_rtk_corrections` | `beginRtkCorrections`, `sendRtkCorrections`, `endRtkCorrections` |
| Raw/smoothed navigation | `rtkNavigationStatus()` | `rtk_navigation_status` | `rtkNavigationStatus` |
| Navigation events | `parseRtkNavigationStatus(frame)` | `parse_rtk_navigation_status` | `readRtkNavigation` |
| Rover raw RTCM3 | `startRoverRtcm`, `stopRoverRtcm`, `parseRoverRtcmChunkView` | `start_rover_rtcm`, `stop_rover_rtcm`, `parse_rover_rtcm_chunk_view` | Not exposed in this local interface |

## GNSS

Read `time_synced` for live external GNSS lock. PPS detection/validity alone is
not proof of UTC synchronization. Check position/fix/DOP validity before using
coordinates, satellite counts and DOP; `nmea_age_ms` and `nmea_update_count`
indicate freshness. No new GNSS data must not be interpreted as a new fix.
Latitude/longitude are signed degrees × 1e7, GGA altitude and geoid separation
are millimetres, DOP is ×1000, UTC is milliseconds of day. GGA altitude is MSL;
ellipsoidal height is MSL altitude plus geoid separation.

`pps_high_width_us`, `pps_min_high_us`, `pps_detected` and `pps_valid` remain
public. When `offset_fresh` is true, `message_pps_offset_us` is the first valid
RMC delay following the corresponding PPS, constrained to 0..800000 us. Do
not display a stale offset as a current measurement. Private RK-PPS precision
diagnostics are not part of either public SDK.

Host GNSS UART baud is configured through `DeviceConfiguration` and its GNSS
field mask; common rates including 115200, 460800 and 921600 are supported.
The external connector's supported mode is `SensorBoardMaster`; the retired
`PpsNmeaOutput` wire value is rejected by Agent 1.1.0.

## CORS input and replay

The SDK does not log into CORS. The application owns NTRIP authentication,
reconnects and GGA uploads, and passes the received **raw RTCM2.x/RTCM3.x bytes**
to begin/send/end. Send chunks no larger than 16 KiB. Other formats are not
supported; inspect `correction_format`, error codes and counters. These bytes
feed the RK solver, not the UM960 or Sensor Board.

Applications generate GGA from current valid device GNSS data; do not invent
position/fix or treat stale data as fresh. Record rover observations and base
corrections separately with timestamps for algorithm replay. Host rover output
contains complete CRC-validated RTCM3 frames and excludes NMEA. Returned chunk
views reference frame memory: copy bytes before releasing/replacing the frame.

## RTK results

`solution_valid` gates raw latitude/longitude/ellipsoidal height;
`smoothed_position_valid` gates the independent dynamics-enabled result.
Never overwrite the raw series with smoothed values. Each has its own epoch,
solution quality and E/N/U standard deviations in metres. Subtract the solution
epoch from **device UTC**, not an unsynchronized host clock, to display age.
FIX/FLOAT transition gates, jump/gap resets and counters are reported. SINGLE
fallback does not replace RTK positions. A valid old solution is not necessarily
a fresh solution: always check epoch age and counters.

Raw geographic coordinates are angular degrees; height and uncertainties are
metres. Convert to ENU only with an explicit, shared origin; do not confuse
position values with precision estimates. Receiver output rate is not a promise
that every epoch produces a fresh RTK solution.

## Executable examples

- Linux/macOS Host: `build/examples/prism-gnss-rtk-status` (read-only snapshot).
- Windows: `windows_runtime_api_examples.cpp` validates all 57 API pointers;
  the compile-only catalogue shows each call without changing a device.
- RK device: [capture and GNSS examples](rk-local-sdk.md).

Client construction/open never sets time. Sensor Board owns the timeline; RK
follows it and provides Ethernet PTP. The Agent rejects host time-setting while
external GNSS is locked. GNSS unlock does not imply sensor timestamps lose their
common Sensor Board time base.
