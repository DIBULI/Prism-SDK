#!/usr/bin/env python3
"""Verify the Prism SDK package, build every supported example, and run tests."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import platform
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
CONFIGURATION = "Release"
EXPECTED_EXPOSURE_LIMIT = b"camera exposure time must be 50..995000 us"
OBSOLETE_EXPOSURE_LIMIT = b"camera exposure time must be 200..995000 us"


def run(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)


def verify_package() -> None:
    manifest = ROOT / "SHA256SUMS"
    checked = 0
    for line_number, line in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
        if not line:
            continue
        try:
            expected, relative_path = line.split("  ", 1)
        except ValueError as error:
            raise RuntimeError(
                f"SHA256SUMS:{line_number}: expected '<hash>  <path>'"
            ) from error
        path = ROOT / relative_path
        if not path.is_file():
            raise RuntimeError(f"missing published file: {relative_path}")
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise RuntimeError(
                f"checksum mismatch for {relative_path}: {actual} != {expected}"
            )
        checked += 1
    if checked == 0:
        raise RuntimeError("SHA256SUMS is empty")
    print(f"Verified {checked} published files.")

    runtime_paths = sorted((ROOT / "runtime").glob("**/libprism_usb_sdk.*"))
    runtime_paths += sorted((ROOT / "runtime/windows-x64").glob("prism_usb_sdk.dll"))
    if not runtime_paths:
        raise RuntimeError("no published SDK runtimes found")
    for path in runtime_paths:
        contents = path.read_bytes()
        relative_path = path.relative_to(ROOT)
        if EXPECTED_EXPOSURE_LIMIT not in contents:
            raise RuntimeError(
                f"runtime lacks the 50 us exposure limit: {relative_path}"
            )
        if OBSOLETE_EXPOSURE_LIMIT in contents:
            raise RuntimeError(
                f"runtime contains the obsolete 200 us limit: {relative_path}"
            )
    print(f"Verified exposure-limit semantics in {len(runtime_paths)} runtimes.")


def host_kind() -> str:
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Linux" and machine in {"x86_64", "amd64", "aarch64", "arm64"}:
        return "linux"
    if system == "Darwin" and machine == "arm64":
        return "macos"
    if system == "Windows" and machine in {"amd64", "x86_64"}:
        return "windows"
    raise RuntimeError(f"unsupported host for this SDK package: {system} {machine}")


def expected_targets(kind: str) -> list[str]:
    if kind == "windows":
        return [
            "prism-device-info-time-sync",
            "prism-windows-runtime-api-examples",
        ]
    return [
        "prism-device-info-time-sync",
        "prism-camera-imu-capture",
        "prism-client-api-examples",
        "prism-configuration-api-examples",
        "prism-lidar-capture",
        "prism-parser-api-examples",
        "prism-stream-api-examples",
    ]


def configure(build_dir: Path, kind: str, generator: str | None) -> None:
    command = ["cmake", "-S", str(ROOT), "-B", str(build_dir)]
    if kind == "windows":
        command.extend(["-G", generator or "Visual Studio 17 2022", "-A", "x64"])
    else:
        command.extend(
            [
                f"-DCMAKE_BUILD_TYPE={CONFIGURATION}",
                "-DCMAKE_CXX_FLAGS=-Werror",
            ]
        )
        if kind == "macos":
            command.extend(
                [
                    "-DCMAKE_OSX_ARCHITECTURES=arm64",
                    "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0",
                ]
            )
    run(command)


def run_device_tests(
    executable_dir: Path,
    kind: str,
    capture_seconds: int,
    lidar_model: str | None,
    settle_seconds: float,
) -> None:
    suffix = ".exe" if kind == "windows" else ""
    run([str(executable_dir / f"prism-device-info-time-sync{suffix}")])
    if kind == "windows":
        if lidar_model is not None:
            raise RuntimeError("LiDAR capture example is not available on Windows")
        print("Windows device-information example passed.")
        return

    # Closing a libusb process does not necessarily disable/re-enable the
    # FunctionFS gadget immediately.  Give the Agent enough time to retire the
    # previous authenticated session before the next standalone example claims
    # the capture owner.  This keeps the automation deterministic without
    # retrying or hiding an actual capture failure.
    if settle_seconds > 0:
        print(f"Waiting {settle_seconds:g}s for USB session release.", flush=True)
        time.sleep(settle_seconds)
    run(
        [
            str(executable_dir / "prism-camera-imu-capture"),
            "--seconds",
            str(capture_seconds),
        ]
    )
    if lidar_model is not None:
        if settle_seconds > 0:
            print(
                f"Waiting {settle_seconds:g}s for USB session release.",
                flush=True,
            )
            time.sleep(settle_seconds)
        run(
            [
                str(executable_dir / "prism-lidar-capture"),
                "--model",
                lidar_model,
                "--seconds",
                str(capture_seconds),
            ]
        )
    print("Device-backed example tests passed.")


def build_and_test(
    build_dir: Path,
    kind: str,
    generator: str | None,
    with_device: bool,
    capture_seconds: int,
    lidar_model: str | None,
    device_settle_seconds: float,
) -> None:
    configure(build_dir, kind, generator)
    run(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--config",
            CONFIGURATION,
            "--parallel",
            "--target",
            "prism-all-examples",
        ]
    )
    run(
        [
            "ctest",
            "--test-dir",
            str(build_dir),
            "-C",
            CONFIGURATION,
            "--output-on-failure",
        ]
    )

    executable_dir = build_dir / "examples"
    suffix = ""
    if kind == "windows":
        executable_dir /= CONFIGURATION
        suffix = ".exe"
    targets = expected_targets(kind)
    for target in targets:
        executable = executable_dir / f"{target}{suffix}"
        if not executable.is_file():
            raise RuntimeError(f"missing built example: {executable}")
        if kind != "windows" and not os.access(executable, os.X_OK):
            raise RuntimeError(f"example is not executable: {executable}")
    print(f"Built and verified {len(targets)} example targets for {kind}.")
    if with_device:
        run_device_tests(
            executable_dir,
            kind,
            capture_seconds,
            lidar_model,
            device_settle_seconds,
        )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=ROOT / "build-all-examples",
        help="CMake build directory (default: %(default)s)",
    )
    parser.add_argument(
        "--generator",
        help="CMake generator override (primarily for Windows)",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="verify SHA256SUMS without configuring or building",
    )
    parser.add_argument(
        "--with-device",
        action="store_true",
        help="also run the safe device information and capture examples",
    )
    parser.add_argument(
        "--capture-seconds",
        type=int,
        default=5,
        help="duration for each device-backed capture (default: %(default)s)",
    )
    parser.add_argument(
        "--lidar-model",
        choices=("mid360", "mid360s"),
        help="also test LiDAR capture with the explicitly selected model",
    )
    parser.add_argument(
        "--device-settle-seconds",
        type=float,
        default=3.0,
        help=(
            "wait between standalone device examples so FunctionFS can "
            "release the previous session (default: %(default)s)"
        ),
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    if arguments.capture_seconds <= 0:
        raise RuntimeError("--capture-seconds must be greater than zero")
    if arguments.device_settle_seconds < 0:
        raise RuntimeError("--device-settle-seconds cannot be negative")
    if arguments.lidar_model is not None and not arguments.with_device:
        raise RuntimeError("--lidar-model requires --with-device")
    verify_package()
    if not arguments.verify_only:
        build_and_test(
            arguments.build_dir.resolve(),
            host_kind(),
            arguments.generator,
            arguments.with_device,
            arguments.capture_seconds,
            arguments.lidar_model,
            arguments.device_settle_seconds,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1) from error
