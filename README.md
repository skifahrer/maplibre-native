# Nightly iOS builds of MapLibre Native terrain

This fork carries no MapLibre code, only small patches in `patches/` that the build
applies after the merge (for now, an iOS switch for the terrain load mode the core
already has). Its default branch holds one workflow,
`terrain-ios`, which every night checks maplibre/maplibre-native's
`feature/terrain-3d` and `main` branches and, when either has moved, merges `main`
into `feature/terrain-3d`, builds the iOS framework from that and publishes a
`terrain-<terrain commit>-main-<main commit>-p<patches hash>` release:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

Run it by hand from the Actions tab (`force` rebuilds the same commit).
