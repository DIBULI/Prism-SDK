# Device information and time synchronization example

`prism-device-info-time-sync` demonstrates the supported application lifecycle:

1. enumerate Prism USB devices;
2. open the first device;
3. read versions and device health;
4. optionally synchronize device time to the host;
5. close the device cleanly.

Build from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

It can also be built as a standalone example:

```bash
cmake -S examples -B build-example -DCMAKE_BUILD_TYPE=Release
cmake --build build-example --config Release
```

Read device information only:

```bash
./build/examples/prism-device-info-time-sync
```

Set and verify device time:

```bash
./build/examples/prism-device-info-time-sync --sync-time
```

The second command is an administrative operation. The host clock must be
correct, and all Camera, IMU, and LiDAR streams must be stopped.
