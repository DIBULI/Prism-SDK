# Independent sensor streams

An absent camera, IMU, LiDAR, or GNSS/RTK receiver does not stop the other
streams. A real shared USB/socket disconnect, explicit stop, or Agent shutdown
still ends the session. No synthetic samples or repeated old images are emitted.

## USB C++ applications

Use `prism::capture::CameraFrameAssembler` from
`<prism/usb/camera_assembler.hpp>`. Feed VideoChunk events to `ingest()` and
VideoMeta events to `addMetadata()`. Call `expire()` regularly, including when
a read times out. A newer frame or a one-second assembly inactivity timeout
retires an incomplete set. Complete images from that set are returned in
`partial_frames`; missing/corrupt image vectors remain empty.

Send one `sendVideoAck(id)` for each `discarded_incomplete_frame_ids` entry.
Those IDs include partial sets: do not acknowledge `partial_frames` again.
A completed set returned by `ingest()` or `addMetadata()` needs its own ACK.
The `video_stats.cpp` example demonstrates these rules and keeps IMU processing
independent. Preserve the assembler across read timeouts.

Use `camera_mask` and each image's size to determine which camera data exists.
A missing metadata timestamp is zero; never replace it with host arrival time
or an encoder timestamp and claim measurement-time synchronization.

`isReadTimeout()` recognizes the current SDK's platform-specific timeout text.
Only use it for stream-read exceptions. Other transport/protocol errors must
still be handled as failures, not swallowed indefinitely.

## RK-local C++ applications

`Client::readFrameSet()` performs assembly and ACK management internally. Use
`FrameSet::cameraMask()`, `complete()`, and `image[i].size`; skip empty slots.
A timeout returns `nullopt` without stopping any stream. Keep the normal
keepalive enabled so idle pending sets also expire. IMU and raw-event queues
continue independently.

## Sensor Board and recordings

The matching firmware accepts any initialized camera subset. Detecting a lost
camera and restoring the active set can cause a bounded recovery gap. Missing
slots contain no usable image; do not repeat an old frame as a replacement.
Unified exposure uses active-camera feedback, and a missing camera does not
indefinitely hold exposure or gain updates on the others.

The current firmware also withholds each IMU's samples until they are internally
aligned. Temporary IMU silence during startup/re-alignment does not stop other
streams. Withheld samples are not replayed. Internal alignment does not require
GPS and must not be mistaken for UTC lock; see [time semantics](time-sync.md).
Use the current matching firmware package for these behaviors.

Viewer/Web show missing-source warnings and keep acquisition running. They
write only received data and mark recordings with missing camera sets or
missing selected streams incomplete. Timestamp regressions and disk failures
remain recording-integrity errors; a failed recording does not stop capture.
