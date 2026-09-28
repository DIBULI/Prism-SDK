# Prism system update package

[Documentation index](../README.md)

<!-- page-toc -->
- [Archive layout](#archive-layout)
- [Obtain and inspect a package](#obtain-and-inspect-a-package)
- [Upgrade sequence](#upgrade-sequence)
- [Host SDK API](#host-sdk-api)
<!-- /page-toc -->

Prism system upgrades are distributed as one ZIP file. The user must supply a
package containing both the RK agent and the sensor-board firmware. The public
Host SDK and Viewer do not expose standalone sensor-board firmware upgrade.

## Archive layout

The ZIP root must contain exactly these three files:

```text
manifest.ini
prism-agent
BOOT.BIN
```

Directories, duplicate entries, renamed component files, and additional files
are rejected. `manifest.ini` format version 1 contains:

```ini
format=prism-system-update
format_version=1
package_version=1.2.0
agent_file=prism-agent
agent_version=1.2.0
agent_sha256=<64 lowercase hexadecimal characters>
sensor_board_file=BOOT.BIN
sensor_board_version=0.4.27
sensor_board_sha256=<64 lowercase hexadecimal characters>
```

The embedded `PRISM_AGENT_VERSION` marker must equal `agent_version`. Both
images are extracted with bounded sizes and verified against their manifest
SHA-256 before the first device write.

## Obtain and inspect a package

Use a publisher-provided combined Agent + Sensor Board ZIP. This binary SDK
repository does not include firmware sources or package-creation tools. Do not
rename an image ZIP or a single BOOT.BIN into an update package.

Inspect a package without opening a device:

```cpp
#include <prism/usb_sdk.hpp>
const auto package = prism::inspectSystemUpgradePackage(package_path);
// Present package.agent_version and package.sensor_board_version for approval.
```

Linux/macOS direct C++ consumers and RK-local use the public helper. Windows
runtime-loaded clients use the matching function table; see the
[Windows examples](../examples/interfaces.md#windows-runtime-api-v18).

## Upgrade sequence

The device must be open and camera, IMU and LiDAR streaming must be stopped.

1. The Host SDK validates the entire ZIP, manifest, embedded agent version, and
   both SHA-256 values.
2. Using the currently matching SDK/Agent pair, the SDK compares the reported
   Sensor Board version with the package. A known matching version is skipped;
   otherwise the SDK stages `BOOT.BIN` on RK and relays it to Sensor Board.
3. When flashing is needed, Sensor Board commits only after QSPI update-slot
   read-back succeeds. A same-version skip sends no OTA transaction.
4. The SDK uploads and commits the agent as the final transaction.
5. If the target version changed, close the old host application and reconnect
   only with the Host SDK whose semantic version exactly matches the new agent.

When Sensor Board is flashed, its image crosses two links, so progress counts
host-to-RK staging and RK-to-sensor-board transfer separately. A same-version skip
counts zero Sensor Board transfer bytes and reports `SkippedSameVersion`.
A sensor-board failure prevents the
agent replacement from starting; keep the package and retry after correcting
the connection or power problem.

Do not remove power while either component is committing. SHA-256 protects
against accidental corruption, but it is not publisher authentication. A
production trust model should add a signed manifest and verify it before step
1.

## Host SDK API

```cpp
auto info = prism::inspectSystemUpgradePackage(package_path);

auto result = client.upgradeSystem(
    package_path,
    {},
    [](const prism::SystemUpgradeProgress& progress) {
      // Show progress.phase and completed_bytes / total_bytes.
    });
```

`SystemUpgradeResult::complete` is true when the Sensor Board commit is
verified (or its matching version was skipped) and the final Agent commit is accepted. For a same-version reinstall
with restart verification enabled, the restarted process must also be
verified. Component-level update frames remain an internal transport detail
and are not standalone public upgrade workflows.
