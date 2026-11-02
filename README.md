# iOS builds of MapLibre Native terrain

This fork carries no MapLibre code, only what our build adds to it:

- `upstream.env`: the commits of maplibre/maplibre-native's `feature/terrain-3d` and `main`
  we build. Nothing else of upstream reaches us.
- `patches/`: our changes, applied after `main` is merged into `feature/terrain-3d` (iOS
  switches for settings the core already has, terrain fixes, upstream fixes not merged yet).
- `tests/ios/`: simulator tests of what the patches promise the app, run as
  `//platform/ios/riki-test:riki_test` inside the upstream tree.

## Upstream changes come in by pull request

Every night `upstream` checks upstream's two branches. When either has moved, it opens a pull
request to `master` that moves the pins in `upstream.env` (or moves its open one further), with
links to upstream's changes. Nothing is merged on its own.

`terrain-ios` runs on that pull request: it merges, patches and builds the framework, then runs
upstream's iOS tests (`//platform/ios/test:ios_test`) and ours on an iOS simulator. A patch that
no longer applies or a red test shows there, before anything ships. Fix the patch on that
pull request's branch; merge it only when `terrain-ios` is green on its head.

## Releases

A push to `master` runs `terrain-ios` again and publishes a
`terrain-<terrain commit>-main-<main commit>-p<patches hash>` release:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

Riki Maps' `maplibre-terrain` workflow then opens its own pull request for that release. Run
`terrain-ios` by hand from the Actions tab to rebuild (`force` releases the same pins again).
