# Runtime camera exposure

[Documentation index](../README.md)

<!-- page-toc -->
- [SDK API](#sdk-api)
- [Current wire format](#current-wire-format)
<!-- /page-toc -->

Camera exposure is runtime-only. It is not part of
`DeviceConfiguration`, is never written to `/var/lib/prism`, and resets to the
device defaults after an agent or sensor-board restart. Reads and writes are
allowed while acquisition is active.

Each SC130GS camera has an independently adjustable sensor gain:

- automatic mode runs the PL-managed RAW8 brightness loop and adjusts TRIG0
  exposure time first, raises gain only after reaching the exposure ceiling,
  and lowers gain before shortening exposure when reducing brightness;
- `target_brightness` is one shared 8-bit target for all automatic cameras
  and defaults to `35`;
- manual mode uses an independent `exposure_time_us` for each camera.
- `gain_x1024` accepts `1024..126976` (`1x..124x`) in multiples of `32`
  (`1/32x` steps); the default is `1024` (`1.0x`), matching register `0x0020`.

The shared runtime limits default to `50..995000 us` and `1x..124x`.
`max_exposure_time_us` is the user ceiling; the Agent separately reports the
effective ceiling after applying the current FPS headroom. The complete
exposure configuration and its limits are cached while acquisition is idle,
so an idle setter returns without waiting for a sensor-board command. They are
sent to the sensor board during the next capture preparation, after the
matching FPS has been configured and before acquisition is enabled. During an
active capture they are applied live.

The maximum exposure follows the active camera rate as
`floor(1000000 / camera_fps) - 5000` microseconds. Supported
integer 1-through-30 fps configurations range from 995000 us at 1 fps to
28333 us at 30 fps. The 5 ms headroom is kept
for sensor readout and transfer before the next trigger.

The PL metric is the mean code value of the complete `1280x1024` RAW8 Bayer
frame, not post-ISP luma. For SC130GS `BGGR`, each 2x2 cell contributes
`(B + G + G + R) / 4`; the Bayer byte order therefore does not need to be
rearranged for this full-frame statistic.

## SDK API

### Unified automatic exposure (four cameras)

Host and RK-local share exactly the same API:

```cpp
prism::ExposureConfiguration group;
group.unified_automatic = true; // false restores independent automatic control
auto applied = client.setExposureConfiguration(
    group, prism::kExposureFieldUnifiedAutomatic);
```

This masked update selects automatic mode on all four cameras and preserves
their retained manual values and gains. Manual per-camera changes are rejected
until unified mode is disabled. The default is independent control. This is
runtime-only, not a saved device setting. Viewer and Web expose both choices.

The current matching firmware uses **the active cameras** for unified exposure;
a missing camera does not indefinitely hold the other cameras' brightness control.
Exposure feedback must belong to the same measurement group. All active cameras
share an exposure duration, while analog gains remain independent.

The highlight policy permits up to approximately **20% saturated pixels** before
highlight protection reduces brightness. This is a control threshold, not a
promise of zero clipping or image quality. Exposure/gain limits still apply.
Automatic gain changes wait for applied feedback and are limited to at most
**five updates per second**, with smaller steps near the brightness target and
faster response to large lighting changes. Missing-camera recovery does not
block gain updates for the remaining cameras. These behaviors require the
current 0.4.27 firmware package, not merely an unchanged version string.

All four physical TRIG0 widths and metadata exposure values match, including
mode transitions: the largest per-camera readout guard is applied to **all**
four. A pending exposure is only latched at the next common trigger; an in-flight
pulse is not truncated. These protections may require several frames to settle.
Readout safety can temporarily exceed a newly lowered requested exposure limit,
with the actual duration reported in metadata.

This protects significant highlights, not every individual specular pixel. It
cannot guarantee zero clipping under arbitrary lighting; minimum exposure/gain
and scene dynamic range are physical limits. Darker views may remain dark.

Requires Sensor Board **EX4** support, exposure wire payload **v3** (44 bytes,
field-mask bit 5; former reserved u16 at byte 10: bit 0 = unified) and matched
SDK headers/libraries (**Runtime ABI 18**). EX3 retains independent mode. Mode
changes are acknowledged by Sensor Board even when idle; an older board rejects
EX4 and Agent leaves its previous mode intact instead of reporting success.
An idle mode change first applies the configured camera/IMU rates (without
starting acquisition), so the EX4 exposure ceiling matches the selected FPS.
Unrelated idle exposure/limit changes retain the cached-until-capture behavior
described above. Rebuild both Host and RK-local clients with the new headers.

### Existing exposure controls

```cpp
ExposureConfiguration cameraExposure();

ExposureConfiguration setExposureConfiguration(
    const ExposureConfiguration& configuration,
    uint32_t field_mask = kExposureFieldAll);

ExposureConfiguration setAutoExposureTargetBrightness(
    uint8_t target_brightness);

ExposureConfiguration setCameraExposure(
    uint8_t camera_index,
    const CameraExposureConfiguration& exposure);

ExposureLimits cameraExposureLimits();

ExposureLimits setCameraExposureLimits(
    const ExposureLimits& limits,
    uint32_t field_mask = kExposureLimitsFieldAll);
```

Supported ranges:

| Field | Values |
| --- | --- |
| `target_brightness` | `1..255`; shared by cameras 0 through 3 |
| `CameraExposureMode` | `Automatic` or `Manual` |
| `exposure_time_us` | `50..(floor(1000000 / camera_fps) - 5000)`; independent per camera |
| `gain_x1024` | `1024..126976` in steps of `32`; independent per camera |
| `min_exposure_time_us` | `50..effective_max_exposure_time_us`; shared |
| `max_exposure_time_us` | `50..995000`; shared user ceiling |
| `effective_max_exposure_time_us` | read-only `min(max_exposure_time_us, floor(1000000 / fps) - 5000)` |
| `min_gain_x1024` / `max_gain_x1024` | `1024..126976` in steps of `32`; shared |

Every setter returns the complete state read back from the agent. For an atomic
multi-camera update, read the current state, modify it, then call
`setExposureConfiguration()`.

```cpp
auto exposure = client.cameraExposure();
exposure.target_brightness = 144;
exposure.automatic_camera_mask = 0b0011;  // camera 0/1 PL auto, 2/3 manual
exposure.manual_exposure_time_us[2] = 250;
exposure.manual_exposure_time_us[3] = 500;
exposure.gain_x1024[2] = 3072;  // 3.0x
exposure = client.setExposureConfiguration(exposure);

auto limits = client.cameraExposureLimits();
limits.min_exposure_time_us = 500;
limits.max_exposure_time_us = 20000;
limits.min_gain_x1024 = 1024;   // 1.0x
limits.max_gain_x1024 = 8192;   // 8.0x
limits = client.setCameraExposureLimits(limits);
```

## Current wire format

`EXPOSURE_GET` (`0x34`) has no payload. `EXPOSURE_SET` (`0x35`) carries the
fixed 44-byte payload below. Both return `EXPOSURE_RESPONSE` (`0xb3`) with the
same layout and a full field mask.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | protocol version, exactly `3` |
| 2 | 2 | payload size, exactly `44` |
| 4 | 4 | field mask: bit 0 target; bits 1..4 camera 0..3; bit 5 unified mode |
| 8 | 1 | automatic-camera mask; bits 0..3 only |
| 9 | 1 | shared automatic-exposure target brightness |
| 10 | 2 | unified mode: `0` or `1`; all other bits must be zero |
| 12 | 16 | `manual_exposure_time_us[4]`, little-endian `u32` |
| 28 | 16 | `gain_x1024[4]`, little-endian `u32` |

The current SDK sends and reads the 44-byte version-3 exposure payload.
Unknown field bits, invalid mode/camera-mask values and out-of-range settings
are rejected. Unified mode requires the automatic-camera mask to include all
four logical slots; hardware availability is evaluated separately. Applications
should use the public setters rather than constructing older payload layouts.

`EXPOSURE_LIMITS_GET` (`0x36`) has no payload.
`EXPOSURE_LIMITS_SET` (`0x37`) and `EXPOSURE_LIMITS_RESPONSE` (`0xb4`) use the
same fixed 28-byte version-2 payload:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 2 | limits payload version, exactly `2` |
| 2 | 2 | payload size, exactly `28` |
| 4 | 4 | field mask: min/max exposure and min/max gain bits |
| 8 | 4 | minimum exposure, microseconds |
| 12 | 4 | user maximum exposure, microseconds |
| 16 | 4 | effective maximum exposure, microseconds; read-only in a set request |
| 20 | 4 | minimum gain, x1024 |
| 24 | 4 | maximum gain, x1024 |
