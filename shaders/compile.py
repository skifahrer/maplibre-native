#!/usr/bin/env python3
"""Compiles every variant variants.py wrote into a .metallib of its own, for one SDK."""
import concurrent.futures
import os
import pathlib
import subprocess
import sys

MINIMUM = {"iphoneos": "-mios-version-min=15.5", "iphonesimulator": "-mios-simulator-version-min=15.5"}


def main(variants, sdk, out):
    out.mkdir(parents=True, exist_ok=True)
    lines = (variants / "variants.tsv").read_text().splitlines()

    def compile(line):
        file, shader, _, _, *defines = line.split("\t")
        # the options mtl::Context::createProgram hands newLibraryWithSource
        command = ["xcrun", "-sdk", sdk, "metal", "-std=metal2.4", "-ffast-math", MINIMUM[sdk],
                   *[f"-D{d}" for d in defines], "-o", str(out / f"{file}.metallib"),
                   str(variants / f"{shader}.metal")]
        done = subprocess.run(command, capture_output=True, text=True)
        return file, done.returncode, done.stderr

    failed = 0
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
        for file, code, errors in pool.map(compile, lines):
            if code:
                failed += 1
                print(f"{file}: {errors}", file=sys.stderr)
    size = sum(f.stat().st_size for f in out.glob("*.metallib"))
    print(f"{sdk}: {len(lines) - failed} of {len(lines)} variants, {size / 1e6:.1f} MB")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main(pathlib.Path(sys.argv[1]), sys.argv[2], pathlib.Path(sys.argv[3]))
