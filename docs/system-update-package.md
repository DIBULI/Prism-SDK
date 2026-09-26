# Prism system update package

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

## Create a package

Use the repository script so filenames, versions, hashes, compression, and
manifest fields remain deterministic:

```sh
python scripts/create_system_update_package.py \
  --agent ../dist/sbin/prism-agent \
  --sensor-board /path/to/BOOT.BIN \
  --package-version 1.2.0 \
  --sensor-board-version 0.4.27 \
  --output prism-system-update-2026.07.24.zip
```

Inspect the result without opening a USB device:

```sh
usb-sdk/build-msvc/prism-system_upgrade.exe \
  --inspect prism-system-update-2026.07.24.zip
```

## Upgrade sequence

The device must be open and camera/IMU streaming must be stopped.

1. The Host SDK validates the entire ZIP, manifest, embedded agent version, and
   both SHA-256 values.
2. Using the currently matching Host SDK/agent pair, the SDK stages
   `BOOT.BIN` on RK and relays it through the sensor-board control link.
3. The sensor-board commits only after QSPI update-slot read-back succeeds.
4. The SDK uploads and commits the agent as the final transaction.
5. If the target version changed, close the old host application and reconnect
   only with the Host SDK whose semantic version exactly matches the new agent.

The sensor-board image crosses two links, so progress counts host-to-RK staging
and RK-to-sensor-board transfer separately. A sensor-board failure prevents the
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

`SystemUpgradeResult::complete` is true when the sensor-board commit is
verified and the final agent commit is accepted. For a same-version reinstall
with restart verification enabled, the restarted process must also be
verified. Component-level update frames remain an internal transport detail
and are not standalone public upgrade workflows.
