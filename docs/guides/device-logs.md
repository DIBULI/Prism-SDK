# Device log ZIP export

Agent 1.4.0+ can expose the optional read-only device-log extension. Viewer uses
the existing authenticated USB or network connection; no SSH password or second
connection is needed. An older Agent without the command remains usable, but
cannot export device logs. Supporting a device for upgrade does not require this
extension.

Stop acquisition/recording first. Agent independently rejects exports while any
camera, IMU, LiDAR or upgrade transfer is active; exporting never issues STOP or
changes power, capture settings, journal configuration or recorded datasets.

Viewer: **Open Log → Export device logs ZIP…**. The SDK reads four fixed sections:
current/previous boot Prism service logs and current/previous boot kernel logs.
Collection is newest-first, up to 4000 lines / 1 MiB per section, with a 1.5-second
collector deadline. Viewer compresses on the computer. Web offers the same export
under **Device information → Export device logs ZIP**, with compression on RK.
The authenticated, same-origin `POST /api/device-logs` accepts only an empty JSON
object; the controller holds its idle/download reservation until HTTP transfer
finishes. A stale browser state cannot bypass Agent's capture check.
Known credential/URL lines are removed. Configuration files, datasets and local
Viewer logs are excluded. Review logs before sharing them publicly.

`manifest.json` records the export boot, device monotonic/wall clocks, Agent/SDK
version, serial when available, exit status and per-section truncation,
unavailability, timeout and redaction. Missing previous-boot persistence is a
partial export, not a complete history. Unsynchronized device wall time is not
treated as trusted UTC. Changing boot or Agent process midway aborts the archive.
Cancellation, malformed replies and transport failure do not save a partial ZIP.
Viewer saves atomically to Unicode paths, including Chinese names.

Wire v1: command `0x56`, response `0xc8`. Request is 8 bytes: u16 version=1,
u16 size=8, u32 section 0..3. Response is 80-byte header plus <=1 MiB text:
u16 version=1, u16 header=80, u32 section, u32 flags (truncated=1,
unavailable=2, timeout=4, redacted=8), u32 exit status, u32 bytes, u32 IEEE CRC32
of the entire payload with this CRC field zero, 36 ASCII boot UUID bytes,
u64 device monotonic ms, u64 device realtime ms, u32 reserved=0. All integers
are little-endian. The caller cannot supply paths, units, shell commands or limits.

The Host SDK exposes `Client::deviceLogArchive(progress, cancel)` and the optional
`prism_usb_sdk_get_device_log_api(1)` runtime extension. Core runtime ABI is unchanged.
