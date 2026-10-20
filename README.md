# Nightly iOS builds of MapLibre Native terrain

This fork carries no MapLibre changes. Its releases are iOS builds of
maplibre/maplibre-native's `feature/terrain-3d` branch, made nightly on a Mac when
the branch has moved, one `terrain-<commit>` release per commit:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

The `terrain-ios` workflow makes the same release on a GitHub runner, by hand
from the Actions tab (`force` rebuilds the same commit), when the Mac cannot.
