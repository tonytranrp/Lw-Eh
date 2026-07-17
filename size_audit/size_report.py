#!/usr/bin/env python3
"""Size-audit orchestration for Lw-Eh (research.md Part B §B9).

Diffs a control build (no Lw-Eh) against a with-lweh build to isolate the
library's true incremental footprint from the unavoidable platform runtime
floor (research.md §B6). Never wired into CTest -- size is a budget/trend
signal to read and act on, not a pass/fail correctness gate
(Research/ARCHITECTURE.md).

TODO(Phase 2+): flesh out once a real embedded cross-toolchain is available
in the build environment (see Research/PROGRESS.md environment note -- as of
Phase 0 this machine has no xtensa-esp32-elf-gcc/arm-none-eabi-gcc/avr-gcc).
For now this documents and stubs the intended workflow:
  1. run `<prefix>size -A` on both ELFs
  2. run `<prefix>nm --size-sort -C` on the with-lweh ELF for a top-N table
  3. run `bloaty with_lweh.elf -- control.elf` in diff mode, if bloaty is on PATH
  4. grep the build log for -Wl,--print-gc-sections output to confirm
     dead-code elimination actually fired
  5. print one consolidated report
"""

import argparse
import shutil
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, help="CMake build directory")
    parser.add_argument("--prefix", default="", help="Cross toolchain prefix, e.g. xtensa-esp32-elf-")
    args = parser.parse_args()

    size_tool = f"{args.prefix}size"
    if shutil.which(size_tool) is None:
        print(
            f"error: '{size_tool}' not found on PATH -- install the matching "
            f"cross toolchain first (see Research/ARCHITECTURE.md environment note)",
            file=sys.stderr,
        )
        return 1

    print("TODO(Phase 2+): implement the full workflow described in this file's docstring.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
