# Riki Maps' MapLibre Native

This is maplibre/maplibre-native's `feature/terrain-3d` with `main` merged in, plus our changes as
ordinary commits: iOS switches for settings the core already has, terrain and Metal fixes, and
upstream fixes not merged yet. Riki Maps takes it as a binary release.

What is ours besides those commits:

- `.github/workflows/terrain-ios.yml` and `upstream.yml`; upstream's own workflows stay in the
  tree but are switched off.
- `platform/ios/riki-test/`: simulator tests of what our changes promise the app.
- `CLAUDE.md` and this file.

## Upstream comes in by pull request

`upstream` runs every night. It keeps one pull request open from each of upstream's `main` and
`feature/terrain-3d` into `master`, straight from upstream's branch, and has `terrain-ios` test
each new upstream head merged into `master`. Nothing is merged on its own.

The `terrain-ios` status builds the framework, then runs upstream's iOS tests
(`//platform/ios/test:ios_test`) and ours (`//platform/ios/riki-test:riki_test`) on an iOS
simulator. Merge only when it is green on the head. When upstream conflicts with our changes:
branch off `master`, `git pull https://github.com/maplibre/maplibre-native.git <branch>`,
resolve, and open a pull request from that branch instead.

## Releases

Each push to `master` runs `terrain-ios` and publishes `terrain-<commit>`:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

Riki Maps' `maplibre-terrain` workflow then opens its own pull request for that release.
`terrain-ios` can be run by hand from the Actions tab (`force` rebuilds a released commit).
