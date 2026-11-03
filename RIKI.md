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
`feature/terrain-3d` into `master`, straight from upstream's branch, mentions @skifahrer when it
opens one, and has `terrain-ios` test each new upstream head. Nothing is merged on its own.

`terrain-ios` merges that head into today's `master` on the runner (GitHub's own merge commit of
a fork's branch can lag `master` by hours), builds the framework, and runs upstream's iOS tests
(`//platform/ios/test:ios_test`), ours (`//platform/ios/riki-test:riki_test`) and the core's
terrain tests on an iOS simulator, one at a time. The result is the `terrain-ios` status on the
pull request's head; merge only when it is green. When upstream conflicts with our changes, or a
test shows a clash: branch off `master`, `git pull https://github.com/maplibre/maplibre-native.git
<branch>`, resolve, open a pull request from that branch and close the upstream one.

It runs on Apple-silicon runners (`macos-26`); Intel runners run the core's timing tests too
slowly. When GitHub has no Mac free it drops the job and the status stays pending; the next
nightly `upstream` run starts it again. Start it by hand from the Actions tab (`pr` = the number)
to go sooner.

A pull request or push that changes only `.github/` or `*.md` files is not built: `terrain-ios`
marks it green at once. A change to `terrain-ios.yml` itself is always built.

## Releases

Each push to `master` that changes the framework runs `terrain-ios` and publishes
`terrain-<commit>`:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

Riki Maps' `maplibre-terrain` workflow then opens its own pull request for that release, which
needs an app build, the `ui-maps` suite and 3D on a simulator before it is merged.
`terrain-ios` can be run by hand from the Actions tab (`force` rebuilds a released commit).
