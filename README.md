# Nightly iOS builds of MapLibre Native terrain

This fork carries no MapLibre changes. Its default branch holds one workflow,
`terrain-ios`, which every night checks maplibre/maplibre-native's
`feature/terrain-3d` branch and, when it has moved, builds the iOS framework from
it and publishes a `terrain-<commit>` release:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

Run it by hand from the Actions tab (`force` rebuilds the same commit).
