# Nightly iOS builds of MapLibre Native terrain

Its default branch holds one workflow, `terrain-ios`, which every night checks
maplibre/maplibre-native's `feature/terrain-3d` branch and, when it or this fork has
moved, builds the iOS framework from it and publishes a
`terrain-<upstream commit>-<fork commit>` release.

The build applies `patches/` and ships every Metal shader variant precompiled in
`MapLibre.framework/Shaders/`: compiling them from source on the first frame held
the main thread for seconds on older devices. `shaders/variants.py` writes the
variants, `shaders/compile.py` compiles them, and the patch loads the matching one,
compiling from source only for a variant that is not there.

Each release carries:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

Run it by hand from the Actions tab (`force` rebuilds the same commit).
