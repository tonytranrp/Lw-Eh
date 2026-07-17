#!/usr/bin/env python3
"""Size-audit orchestration for Lw-Eh (research.md Part B Section 9).

Diffs a control build (no Lw-Eh) against a with-lweh build to isolate the
library's true incremental footprint from the unavoidable platform runtime
floor (research.md Section B6). Never wired into CTest -- size is a
budget/trend signal to read and act on, not a pass/fail correctness gate
(Research/ARCHITECTURE.md).

Usage (host-representative, always available -- no cross-toolchain needed):
    cmake --preset size-host
    cmake --build --preset size-host
    python size_audit/size_report.py --build-dir build/size-host

Once a real embedded cross-toolchain is available (see the environment note
in Research/PROGRESS.md -- as of this writing this dev machine has none),
point --with-target at the real embedded example instead:
    cmake --preset esp32-xtensa -DLWEH_BUILD_SIZE_AUDIT=ON
    cmake --build --preset esp32-xtensa
    python size_audit/size_report.py --build-dir build/esp32-xtensa \\
        --prefix xtensa-esp32-elf- --with-target lw_eh_example_esp32_minimal
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional


def find_tool(prefix: str, base: str) -> Optional[str]:
    """Resolve a binutils-family tool. Tries the cross-prefixed name first
    (e.g. xtensa-esp32-elf-size), then falls back to the llvm- prefixed name
    for host-only LLVM setups with no prefix -- this repo's dev machine has
    LLVM tools (llvm-size, llvm-nm) but no plain GNU binutils on PATH (see
    Research/PROGRESS.md environment note)."""
    candidates = [f"{prefix}{base}"]
    if not prefix:
        candidates.append(f"llvm-{base}")
    for candidate in candidates:
        if shutil.which(candidate):
            return candidate
    return None


def find_executable(build_dir: Path, target_stem: str) -> Optional[Path]:
    for suffix in ("", ".exe", ".elf"):
        matches = sorted(build_dir.rglob(f"{target_stem}{suffix}"))
        if matches:
            return matches[0]
    return None


def run_captured(cmd) -> str:
    result = subprocess.run(cmd, capture_output=True, text=True)
    return (result.stdout or "") + (result.stderr or "")


def dec_size(size_tool: str, exe_path: Path) -> int:
    """Total size (text+data+bss) read from the 'dec' column of <size_tool>'s
    default berkeley-style output -- the same number this project's manual
    size probes (firings 2-5, before this script existed) have been reading
    by hand every time."""
    output = run_captured([size_tool, str(exe_path)])
    lines = [line for line in output.strip().splitlines() if line.strip()]
    if len(lines) < 2:
        raise RuntimeError(f"unexpected '{size_tool}' output for {exe_path}:\n{output}")
    parts = lines[-1].split()
    return int(parts[3])


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--build-dir", required=True, help="CMake build directory, e.g. build/size-host")
    parser.add_argument("--prefix", default="", help="Cross toolchain prefix, e.g. xtensa-esp32-elf-")
    parser.add_argument("--control-target", default="lw_eh_size_audit_control")
    parser.add_argument(
        "--with-target",
        default="lw_eh_size_audit_with_lweh_host",
        help="With-Lw-Eh target stem to measure against control; pass "
        "lw_eh_example_esp32_minimal once a real cross-toolchain is used",
    )
    args = parser.parse_args()

    build_dir = Path(args.build_dir)
    if not build_dir.is_dir():
        print(f"error: build dir '{build_dir}' not found -- configure and build it first", file=sys.stderr)
        return 1

    size_tool = find_tool(args.prefix, "size")
    if size_tool is None:
        print(
            f"error: no '{args.prefix}size' (or llvm-size) found on PATH -- "
            f"install the matching toolchain first",
            file=sys.stderr,
        )
        return 1

    control_exe = find_executable(build_dir, args.control_target)
    with_exe = find_executable(build_dir, args.with_target)
    if control_exe is None or with_exe is None:
        print(
            f"error: couldn't find built executables under '{build_dir}' "
            f"(control='{args.control_target}', with='{args.with_target}') -- "
            f"did you run cmake --build on this preset with LWEH_BUILD_SIZE_AUDIT=ON?",
            file=sys.stderr,
        )
        return 1

    control_dec = dec_size(size_tool, control_exe)
    with_dec = dec_size(size_tool, with_exe)
    delta = with_dec - control_dec

    print(f"size tool:    {size_tool}")
    print(f"control:      {control_exe}  ({control_dec} bytes)")
    print(f"with-lweh:    {with_exe}  ({with_dec} bytes)")
    print(f"incremental:  {delta} bytes")
    print("(Lw-Eh's true contribution, isolated from the platform runtime floor -- research.md Section B6)")

    nm_tool = find_tool(args.prefix, "nm")
    if nm_tool:
        print("\ntop 15 symbols by size in the with-lweh binary:")
        output = run_captured([nm_tool, "--size-sort", "--print-size", "-C", str(with_exe)])
        lines = [line for line in output.strip().splitlines() if line.strip()]
        for line in lines[-15:]:
            print(f"  {line}")
    else:
        print(f"\n(no '{args.prefix}nm' or llvm-nm found -- skipping symbol breakdown)")

    bloaty = shutil.which("bloaty")
    if bloaty:
        print("\nbloaty diff (with-lweh -- control):")
        print(run_captured([bloaty, str(with_exe), "--", str(control_exe)]))
    else:
        print(
            "\n(bloaty not found on PATH -- skipping diff-mode breakdown; "
            "see research.md Section B9 for what it would add)"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
