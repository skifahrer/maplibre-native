#!/usr/bin/env python3
"""Writes MapLibre's Metal shaders, and every define set the renderer can ask of them, for precompiling."""
import itertools
import pathlib
import re
import sys

RAW = re.compile(r'R"([^(\s]*)\((.*?)\)\1"', re.S)
CONSTANT = re.compile(r'constexpr\s+auto\s+(\w+)\s*=\s*(@RAW\d+@|"[^"]*"|\w+)\s*;')
STRUCT = re.compile(r'struct\s+ShaderSource<\s*BuiltIn::(\w+)\s*,\s*gfx::Backend::Type::Metal\s*>\s*\{(.*?)\n\};', re.S)
UNIFORM = re.compile(r'(?<![A-Za-z0-9_])HAS_UNIFORM_u_([A-Za-z0-9_]+)')
SCALE = re.compile(r'(?<![A-Za-z0-9_])DEVICE_PIXEL_RATIO(?![A-Za-z0-9_])')
# what util::toString(pixelRatio, true) gives on 2x and 3x screens; others compile at runtime
SCALES = ["2.0", "3.0"]


def shaders(root):
    raws, constants, found = [], {}, []
    for header in sorted((root / "include/mln/shaders/mtl").glob("*.hpp")):
        def stash(match):
            raws.append(match.group(2))
            return f"@RAW{len(raws) - 1}@"
        text = RAW.sub(stash, header.read_text())
        for struct in STRUCT.finditer(text):
            members = dict(CONSTANT.findall(struct.group(2)))
            found.append((struct.group(1), members))
        for name, value in CONSTANT.findall(STRUCT.sub("", text)):
            constants[name] = value

    def resolve(value):
        if value.startswith("@RAW"):
            return raws[int(value[4:-1])]
        if value.startswith('"'):
            return value[1:-1]
        return resolve(constants[value])

    common = resolve(constants["prelude"])
    for shader, members in found:
        yield (resolve(members["name"]),
               common + resolve(members["prelude"]) + resolve(members["source"]),
               resolve(members["vertexMainFunction"]),
               resolve(members.get("fragmentMainFunction", '""')))


def main(root, out):
    out.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, source, vertex, fragment in shaders(root):
        (out / f"{name}.metal").write_text(source)
        properties = sorted(set(UNIFORM.findall(source)))
        scales = SCALES if SCALE.search(source) else [None]
        for scale, count in itertools.product(scales, range(len(properties) + 1)):
            for chosen in itertools.combinations(properties, count):
                file = ".".join((name + (f"@{scale}" if scale else ""),) + chosen)
                defines = [f"HAS_UNIFORM_u_{p}=" for p in chosen]
                defines += [f"DEVICE_PIXEL_RATIO={scale}"] if scale else []
                manifest.append("\t".join([file, name, vertex, fragment] + defines))
    (out / "variants.tsv").write_text("\n".join(manifest) + "\n")
    print(f"{len(manifest)} variants of {len(list(out.glob('*.metal')))} shaders")


if __name__ == "__main__":
    main(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
