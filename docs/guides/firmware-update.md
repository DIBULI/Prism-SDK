# Prism system update package

[Documentation index](../README.md)

<!-- page-toc -->
- [Archive layout](#archive-layout)
- [Obtain and inspect a package](#obtain-and-inspect-a-package)
- [Upgrade sequence](#upgrade-sequence)
- [Host SDK API](#host-sdk-api)
- [Agent-managed Sensor Board maintenance](#agent-managed-sensor-board-maintenance)
<!-- /page-toc -->

Prism system upgrades are distributed as one ZIP file. The user must supply a
package containing both the RK agent and the sensor-board firmware. The public
Viewer uses this combined package workflow. The C++ SDK additionally supports
the standalone maintenance workflow below.

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
A refreshed firmware file can have the same displayed version as an older build.
If the publisher requires that replacement, use the standalone maintenance
workflow with an explicit `--force` (or `options.force=true`), after checking the
supplied file's checksum. A successful combined upgrade with a version skip is
not proof that the replacement Sensor Board firmware was installed.
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
and are not the new background maintenance interface below.

## Agent-managed Sensor Board maintenance

With the refreshed Agent/SDK **1.2.0**, stop all capture and finish recording,
but keep Agent/Web running. The image includes an Agent-managed CLI:

```sh
sudo prism-sensor-board-upgrade --version 0.4.27 /usr/share/prism/firmware/sensor-board/BOOT.BIN
prism-sensor-board-upgrade --status
```

`--version` must match the supplied BIN. A fresh known matching board version
skips writing; `--force` bypasses only this skip. Without a version, equal-version
skip is unavailable. `--no-wait` returns after verified upload and commit.
`--status` shows the current/latest maintenance task, not UART diagnostics.
Starting locally requires root. The tool never opens UART or automatically falls
back to the legacy direct-UART updater. Leave TimeSync RTK mode explicitly first.

Host and RK-local C++ clients share identical methods and result types:

```cpp
prism::SensorBoardUpdateOptions options;
options.version = "0.4.27";
options.force = false;
auto task = client.startSensorBoardUpdate("BOOT.BIN", options);
// After commit, the connection may close; reconnect and query the same task.
auto state = client.sensorBoardUpdateStatus(task.task_id);
// state.active(), state.successful(), state.state, state.error_code, state.message
// state.received / total_size: upload; device_received: board transfer bytes.
```

States are `Idle`, `Receiving`, `Flashing`, `Complete`, `Failed`, `Skipped`,
`Aborted`. The Agent validates size and SHA-256 before Flash, owns programming,
read-back and restart/version verification, and rejects conflicting operations.
Web displays progress without stopping its service. This is a new wire extension;
an earlier 1.2.0 Agent build reports unsupported rather than falling back.

RK-local also provides free functions with the same names, plus an optional
control-socket argument. They coexist with an idle Web/capture connection and do
not require `Client::open()`. `sensorBoardUpdateStatus()` also accepts a timeout.
The Windows Runtime API v18 table is unchanged: use direct linked C++ methods
with the matching MSVC SDK for this new maintenance API, not the runtime table.

An incomplete upload expires after 120 seconds without writing Flash. After
commit, disconnecting SSH/USB/Web does not cancel the job; do not remove power
or restart Agent. This is not power-loss/crash continuation. Task status is
in memory until the next task or Agent restart. If a reply is lost, query status
before retrying; never assume timeout means failure. SHA-256 checks integrity,
not publisher authenticity. Use trusted board-specific firmware only.

The first installation of these new binaries still requires one Agent/Web
restart. Sensor Board firmware and its OTA protocol are unchanged.
