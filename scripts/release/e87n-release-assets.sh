#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

target_dir="${1:-bin/targets/mediatek/filogic}"
output="${2:-$target_dir/release-sha256sums}"

files='immortalwrt-mediatek-filogic-edgepi_e87n-initramfs-kernel.bin
immortalwrt-mediatek-filogic-edgepi_e87n-squashfs-sysupgrade.bin
immortalwrt-mediatek-filogic-edgepi_e87n.manifest
profiles.json
config.buildinfo
feeds.buildinfo
version.buildinfo
e87n-build-evidence.txt'

for file in $files; do
	[ -f "$target_dir/$file" ] || {
		echo "missing release artifact: $target_dir/$file" >&2
		exit 1
	}
done

tmp="$output.tmp"
trap 'rm -f -- "$tmp"' EXIT
(
	cd "$target_dir"
	sha256sum $files
) > "$tmp"
mv "$tmp" "$output"
trap - EXIT

echo "release checksums written to $output"
