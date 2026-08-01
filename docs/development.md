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
E87N device selection, and rejects obsolete wrapper/project names. A full image
build remains the release gate:

```sh
make -j"$(nproc)"
sha256sum -c bin/targets/mediatek/filogic/sha256sums
```

For a targeted edit, build the affected package or kernel first, then run the
full image build before merging. Keep generated directories (`build_dir/`,
`staging_dir/`, `tmp/`, `bin/`, and `dl/`) out of Git.

## Hardware boundary

Build and CI checks cannot validate electrical behavior. Before a release is
called board-validated, test sysupgrade/recovery, Ethernet link stability,
framebuffer output, backlight control, and fan behavior on an E87N device.
