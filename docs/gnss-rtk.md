# GNSS and receiver-native RTK

[简体中文](gnss-rtk.zh-CN.md) · [API reference](development-guide.md)

Agent forwards the receiver's measurements and results. The SDK does not run
an RTK solver, manufacture FIX states, or provide a separate smoothed position.

| Purpose | Host and RK-local API |
| --- | --- |
| Live GGA, fix, satellites, PPS/timing freshness | `gnssTimingStatus()` |
| UART/framing/parse diagnostics | `gnssReceptionStatus()` |
| Receiver reports for sky and positions | `gnssObservations(cursor, session)` |
| Module link, 4G, CORS counters and execution state | `timeSyncRtkStatus()` |
| Application / bootloader versions | `timeSyncRtkVersions()` |
| Saved CORS endpoint; password never returned | `timeSyncCorsConfiguration()` |
| Save a full replacement CORS account | `saveTimeSyncCorsConfiguration(config)` |
| Explicit RTK start / stop | `startRtk(options)` / `stopRtk()` |

Use `prism::gnss_plot::Model` from `<prism/usb/gnss_plot.hpp>` with observation
batches. `model.gnss` is GGA; `model.rtk` is the independent ADRNAV receiver
solution. Keep them separate. Check `valid` and freshness against the batch's
device monotonic time. `session` changes reset the model, and `gap` indicates
missing history. GGA UTC is time-of-day; ADRNAV epoch is GPS week/milliseconds,
not a host Unix timestamp. Preserve the original report when full timing matters.

Receiver standard deviations are optional uncertainty estimates in metres, not
a guaranteed confidence percentage. Missing uncertainty remains unavailable.
An absent/stale result is not a transport failure and must not be shown as live.

`startRtk` confirms the requested control transition, not satellite lock or FIX.
Saved, applied, connected and positioning are different states. See
[CORS/RTK control](rtk-module-control.md), [TimeSync modes](timesync-port.md),
[GNSS diagnostics](gnss-reception-status.md) and the
[read-only example](../examples/gnss_rtk_status.cpp).
