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
        command = ["xcrun", "-sdk", sdk, "metal", "-std=ios-metal2.4", "-ffast-math", MINIMUM[sdk],
                   *[f"-D{d}" for d in defines], "-o", str(out / f"{file}.metallib"),
                   str(variants / f"{shader}.metal")]
        done = subprocess.run(command, capture_output=True, text=True)
        return file, shader, done.returncode, done.stderr

    built, skipped = set(), []
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
        for file, shader, code, errors in pool.map(compile, lines):
            if code:
                # MapLibre cannot use a combination its own source does not compile
                skipped.append(file)
                print(f"{file}: {errors.splitlines()[0] if errors else code}", file=sys.stderr)
            else:
                built.add(shader)
    size = sum(f.stat().st_size for f in out.glob("*.metallib"))
    print(f"{sdk}: {len(lines) - len(skipped)} of {len(lines)} variants, {size / 1e6:.1f} MB")
    unbuilt = sorted({line.split("\t")[1] for line in lines} - built)
    if unbuilt:
        sys.exit(f"no variant of {', '.join(unbuilt)} compiled")


if __name__ == "__main__":
    main(pathlib.Path(sys.argv[1]), sys.argv[2], pathlib.Path(sys.argv[3]))
