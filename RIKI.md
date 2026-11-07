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

`upstream` runs every night. For each of upstream's `main` and `feature/terrain-3d` it keeps a
branch of ours, `upstream-main` and `upstream-feature-terrain-3d`: upstream's branch with `master`
merged into it, merged again whenever either moves. It keeps one pull request open from each into
`master` and mentions @skifahrer when it opens one. Nothing is merged on its own.

That pull request's `terrain-ios` run builds the framework from its head, so from upstream's
changes on top of ours, and runs upstream's iOS tests (`//platform/ios/test:ios_test`), ours
(`//platform/ios/riki-test:riki_test`) and the core's terrain tests on an iOS simulator, one at a
time. Merge only when its `terrain-ios` status is green on the head.

When upstream conflicts with our changes, `upstream` pushes upstream's side alone and marks the
head red. Check out the branch, `git merge master`, resolve, build and push; later nightly runs
merge on top of what you resolved.

Pushing upstream's workflow changes, and having that push start the pipeline, takes an
`UPSTREAM_TOKEN` secret: a token with the `repo` and `workflow` scopes. Without it `upstream`
starts `terrain-ios` on the branch itself and fails on any upstream change to `.github/workflows/`.

It runs on Apple-silicon runners (`macos-26`); Intel runners run the core's timing tests too
slowly. When GitHub has no Mac free it drops the job and the status stays pending; the next
nightly `upstream` run starts it again. Run `terrain-ios` by hand on the branch from the Actions
tab to go sooner.

A pull request or push that changes only `.github/` or `*.md` files is not built: `terrain-ios`
marks it green at once. To try a workflow change, run `terrain-ios` by hand on its branch.

## Releases

Each push to `master` that changes the framework runs `terrain-ios` and publishes
`terrain-<commit>`:

- `MapLibre.xcframework.zip`: the dynamic framework, without its dSYMs
- `MapLibre.framework.dSYM.zip`: the device dSYM, for crash symbolication
- `checksum.txt`: the Swift Package Manager checksum of the framework zip

Riki Maps' `maplibre-terrain` workflow then opens its own pull request for that release, which
needs an app build, the `ui-maps` suite and 3D on a simulator before it is merged.
`terrain-ios` can be run by hand from the Actions tab (`force` rebuilds a released commit).

When the release step itself fails (a run the Intel fallback dispatched was refused with HTTP
403), the three files are kept a week as the run's `terrain-<commit>` artifact. Publish them by
hand: `gh run download <run> -n terrain-<commit>`, then `gh release create terrain-<commit>
--target <commit> MapLibre.xcframework.zip MapLibre.framework.dSYM.zip checksum.txt`.
