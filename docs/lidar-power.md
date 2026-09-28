# Independent LiDAR standby and wake

These operations control the radar hardware, not just the application's receive
stream. They require an Agent build containing the LiDAR power command extension;
older 1.2.0 builds reject the new commands as unsupported. A version string alone
does not establish support. Update the complete 1.2.0 package together.

Host `prism::Client` and RK-local `prism::rklocal::Client` share the C++ API:

```cpp
// Use an already opened, idle client. Choose the actual attached radar.
const auto model = prism::LidarModel::Xt32; // or Mid360 / Mid360S
auto before = client.lidarPowerStatus(model, 3000);
auto idle = client.setLidarStandby(model, true, 10000);
auto awake = client.setLidarStandby(model, false, 10000);
// Start capture separately, only when wanted.
```

- First stop all camera, board-IMU and LiDAR capture streams. These methods never
  stop or resume them automatically. Normal capture start/stop behavior is unchanged.
- Target IP and host-interface IP come from saved LiDAR network configuration;
  the interface must already be ready. These calls never change addresses, PTP,
  calibration, return mode or saved network settings.
- MID360 uses IDLE (target 2) and SAMPLING (target 1), then reads current work
  state. MID360S uses the same protocol. Product text is checked for the MID360
  family because some MID360S firmware reports `DevType:Mid-360`; capture still
  uses the actual discovery device type to distinguish MID360 and MID360S.
  HAP's SLEEP (3) is not used.
- XT32 uses the radar's Standby/Operation command and reads back its standby mode.
  This is vendor mode confirmation, not an independent motor-speed or power
  measurement. It is not a power-off command; network electronics remain powered.
- Wake does not begin Agent data capture. Rotor startup and usable point output
  can take additional time after a mode acknowledgment, particularly on XT32.
- Result `state`: Unknown=0, Running=1, Standby=2, Transitioning=3, Error=4.
  `vendor_state` retains MID360 current-work-state or XT32 standby-mode (0/1).
- Timeout is 1..30000 ms, for the whole hardware transaction. SDK transport adds
  2000 ms response margin. Repeated requests for the current confirmed state do
  not repeat the mode write. MID360 read-only UDP queries may be retried within
  that overall deadline when a reply is lost.
- On timeout or invalid/rejected readback, an exception is raised; a command may
  already have reached the radar. Query status before deciding whether to retry.
  Writes are never retried automatically. Unknown/unconfirmed is not Standby.
- Concurrent operations from other controllers should be avoided. Serialize client
  I/O; the Agent serializes power control with capture changes and upgrades.
- Host and RK-local share the C++ methods above. Windows runtime-loaded clients
  use the version-1 `prism_usb_sdk_get_lidar_power_api` extension with `query` and
  `set_standby` callbacks. The existing RuntimeApi v18 layout remains unchanged.
  Check version, table size and function pointers before use; an absent extension
  is unsupported, not a successful operation.

Protocol fixtures cover query, standby, wake, idempotency, transition, rejection,
CRC/identity checks and timeout. Hardware power consumption is not measured by
these APIs.
