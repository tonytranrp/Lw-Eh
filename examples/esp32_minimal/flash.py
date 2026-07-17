#!/usr/bin/env python3
"""Consolidated flash/observe recipe for examples/esp32_minimal on real ESP32
hardware (Research/PROGRESS.md firings 38-46).

This exists because the same multi-step recipe -- regenerate the partition
table from the real default.csv (never the stale prebuilt default.bin, see
commit f4a09c6), convert the prebuilt bootloader ELF with EXPLICIT
--flash_mode/--flash_freq/--flash_size (an omitted --flash_mode silently
defaults to qio against this board's dio-only wiring, see firing 40), convert
this project's own built ELF with the same flags, and flash all four
components at their verified offsets -- was manually re-typed by hand many
times across firings 38-46, by more than one concurrent thread, with several
of that investigation's real bugs (flash-mode mismatch, stale partition
table) stemming directly from a step being silently skipped or gotten wrong
by hand. This script is the single source of truth for that recipe going
forward; the flags/offsets it uses are not new guesses -- they're exactly
what firings 38-46 verified on real hardware.

Usage (from repo root, after `cmake --build --preset esp32-xtensa`):
    python examples/esp32_minimal/flash.py --port COM5
    python examples/esp32_minimal/flash.py --port COM5 --observe-seconds 15

Requires: the xtensa-esp32-elf toolchain and esptool.py already present via
PlatformIO (see durable memory / Research/PROGRESS.md environment notes) --
this script never fetches anything itself, matching research.md B1.
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def find_platformio_packages() -> Path:
    candidates = [Path.home() / ".platformio" / "packages"]
    for candidate in candidates:
        if candidate.is_dir():
            return candidate
    raise SystemExit(
        "error: could not find ~/.platformio/packages -- this script reuses the "
        "already-installed PlatformIO toolchain/framework rather than fetching "
        "anything new (research.md B1); install PlatformIO first if it's genuinely absent."
    )


def run(cmd, **kwargs):
    print(f"+ {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, **kwargs)
    if result.returncode != 0:
        raise SystemExit(f"error: command failed with exit code {result.returncode}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM5")
    parser.add_argument(
        "--build-dir",
        default="build/esp32-xtensa",
        help="CMake build directory containing the built example ELF (default: build/esp32-xtensa)",
    )
    parser.add_argument(
        "--elf",
        default=None,
        help="Path to the built lw_eh_example_esp32_minimal ELF (default: <build-dir>/examples/esp32_minimal/lw_eh_example_esp32_minimal)",
    )
    parser.add_argument(
        "--observe-seconds",
        type=float,
        default=0,
        help="After flashing, open the serial port and print boot output for this many seconds (0 = skip)",
    )
    args = parser.parse_args()

    packages = find_platformio_packages()
    esptool = packages / "tool-esptoolpy" / "esptool.py"
    framework = packages / "framework-arduinoespressif32"
    bootloader_elf = framework / "tools" / "sdk" / "esp32" / "bin" / "bootloader_dio_40m.elf"
    partitions_csv = framework / "tools" / "partitions" / "default.csv"
    boot_app0 = framework / "tools" / "partitions" / "boot_app0.bin"
    gen_esp32part = framework / "tools" / "gen_esp32part.py"

    for path in (esptool, bootloader_elf, partitions_csv, boot_app0, gen_esp32part):
        if not path.is_file():
            raise SystemExit(f"error: expected file not found: {path}")

    elf_path = Path(args.elf) if args.elf else Path(args.build_dir) / "examples" / "esp32_minimal" / "lw_eh_example_esp32_minimal"
    if not elf_path.is_file():
        raise SystemExit(f"error: example ELF not found at {elf_path} -- build it first (cmake --build --preset esp32-xtensa)")

    flash_flags = ["--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB"]

    with tempfile.TemporaryDirectory(prefix="lweh_flash_") as tmp:
        tmp_path = Path(tmp)
        partitions_bin = tmp_path / "partitions.bin"
        bootloader_bin = tmp_path / "bootloader.bin"
        firmware_bin = tmp_path / "firmware.bin"

        # Never the prebuilt tools/partitions/default.bin -- it's stale
        # relative to default.csv in this exact framework version (a real
        # byte-level mismatch caught in firing 38, commit f4a09c6).
        run([sys.executable, str(gen_esp32part), str(partitions_csv), str(partitions_bin)])

        run([sys.executable, str(esptool), "--chip", "esp32", "elf2image", *flash_flags,
             "-o", str(bootloader_bin), str(bootloader_elf)])

        run([sys.executable, str(esptool), "--chip", "esp32", "elf2image", *flash_flags,
             "-o", str(firmware_bin), str(elf_path)])

        run([
            sys.executable, str(esptool), "--chip", "esp32", "--port", args.port,
            "write_flash", *flash_flags,
            "0x1000", str(bootloader_bin),
            "0x8000", str(partitions_bin),
            "0xe000", str(boot_app0),
            "0x10000", str(firmware_bin),
        ])

    if args.observe_seconds > 0:
        import serial  # local import: only needed for this optional step

        print(f"\n--- observing {args.port} for {args.observe_seconds}s ---")
        port = serial.Serial(args.port, 115200, timeout=1)
        end_time = time.time() + args.observe_seconds
        while time.time() < end_time:
            chunk = port.read(256)
            if chunk:
                sys.stdout.buffer.write(chunk)
                sys.stdout.flush()
        port.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
