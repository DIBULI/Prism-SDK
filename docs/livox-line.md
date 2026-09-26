# MID360 / MID360S point line

Host SDK and RK-local SDK expose the same `prism::LidarPoint` fields:

```cpp
const auto batch = prism::parseLidarPointBatch(frame);
for (const auto& point : batch.points) {
  if (point.line_valid) {
    const unsigned line = point.line; // 0, 1, 2, 3
    // Use line with this point's XYZ, intensity/tag and measurement time.
  }
}
```

The Agent assigns `line = original_udp_point_index % 4` for both Cartesian
high/low precision MID360 and MID360S packets, matching
[Livox ROS Driver 2](https://github.com/Livox-SDK/livox_ros_driver2/blob/master/src/comm/pub_handler.cpp).
This is a driver-generated scan-line identifier, not a field read from the
Livox packet, a tag-bit extraction, or the vertical ring of a spinning LiDAR.
Generation happens before queueing and batch merging. Never regenerate line
from the index within an SDK batch, a filtered cloud or a 100 ms ROS frame.

MID point payloads remain 16 bytes. Previously reserved byte 14 is `line`;
byte 15 is flags, bit 0 = valid, all other bits zero. Valid line is 0..3.
Zero/zero denotes unavailable metadata from an older producer, not a measured
line 0. A nonzero line with zero flags and unknown flags are rejected.
This extension applies to the existing MID v1/v2 point payloads and dataset
point records; it does not change their headers, timestamps or bandwidth.

`serializeLidarPoints()` preserves line and validity in recorded data. ROS
PointCloud2 publishes UINT8 `line` at offset 14 and UINT8 `line_valid` at offset
15, retaining the 20-byte point stride and UINT32 nanosecond `offset_time` at
offset 16. RK Web records the original payload, including these bytes.

XT32 remains unchanged: use `ring` 0..31 and explicit `offset_ns`; its MID
`line_valid` is false. Do not substitute MID line for XT32 ring.

Use matching Runtime ABI **18** headers and libraries for both SDK transports.
Rebuild applications; do not copy these headers over old binary SDK packages.
