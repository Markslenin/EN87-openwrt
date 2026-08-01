# Development workflow

The canonical checkout is a complete ImmortalWrt source tree. Keep it on a
case-sensitive Linux filesystem and do not nest it inside a wrapper repository.

## Branches

- `main` is the stable, buildable baseline.
- Use short-lived `codex/*` or topic branches for changes.
- Rebase or fast-forward completed work into `main`; do not keep merged topic
  branches indefinitely.

## Before a full build

```sh
./scripts/feeds update -a
./scripts/feeds install -a
./scripts/ci/e87n-check.sh
git diff --check
```

The check script regenerates `.config` from `configs/e87n.config`, validates the
E87N device selection, validates the optional OpenClash seed, and rejects
obsolete wrapper/project names. The stable profile is `configs/e87n.config`;
policy variants such as `configs/e87n-openclash.config` must remain separate.
A full image build remains the release gate:

```sh
make -j"$(nproc)"
sha256sum -c bin/targets/mediatek/filogic/sha256sums
./scripts/ci/e87n-mt7922-image-check.sh
```

Release builds must start from a clean committed tree. Record both commands in
the release evidence before starting the build:

```sh
test -z "$(git status --porcelain)"
git rev-parse HEAD
```

For a targeted edit, build the affected package or kernel first, then run the
full image build before merging. Keep generated directories (`build_dir/`,
`staging_dir/`, `tmp/`, `bin/`, and `dl/`) out of Git.

## Locked feeds

`feeds.conf.default` pins `packages`, `luci`, `routing` and `telephony` to exact
commits. A release build must not replace those pins with moving branch names.
To update feeds deliberately:

1. Record the desired upstream commits and review their changes.
2. Change all intended pins in one topic-branch commit.
3. Run `./scripts/feeds clean`, `./scripts/feeds update -a` and
   `./scripts/feeds install -a` so an existing checkout cannot retain an older
   pinned worktree.
4. Run the source checks and a full image build before merging.

The generated `feeds.buildinfo` is release evidence, not a substitute for
committing the pins.

## Formal release builds

`.github/workflows/e87n-release-build.yml` performs the heavyweight firmware
build only for a `v*` tag or an explicit manual dispatch. Pull requests retain
the faster source and package-policy checks. The release workflow:

- starts from the checked-out commit and pinned feeds;
- records the commit, profile, configuration digest and feed revisions;
- builds either the stable or OpenClash profile;
- verifies the complete target `sha256sums`;
- verifies the MT7922 manifest, provider uniqueness, modules, firmware and user-space files;
- uploads the E87N images, manifest, buildinfo files, profiles metadata,
  evidence and a release-scoped checksum file.

`release-sha256sums` deliberately lists only uploaded release artifacts. The
buildroot's complete `sha256sums` remains an internal full-target verification
and must not be presented as an attachment-only checksum file.

## Hardware boundary

Build and CI checks cannot validate electrical behavior. Before a release is
called board-validated, test sysupgrade/recovery, Ethernet link stability,
framebuffer output, backlight control, and fan behavior on an E87N device.

## Legacy wrapper disposition

The former outer build repository is an archive, not a build input. It may keep
historical scripts, reports and vendor snapshots, but must not contain or drive
the canonical source as a submodule. Do not copy fixes back into the wrapper or
publish firmware from it.

All maintained source changes, CI, tags and releases belong to this standalone
repository. A future local checkout should use a separate clean directory;
historical wrapper worktrees should be clearly marked read-only.
