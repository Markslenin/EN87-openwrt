#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf -- "$tmp"' EXIT

extract_ipk() {
	ipk="$1"
	dest="$2"
	mkdir -p "$dest"
	case "$(file -b "$ipk")" in
	*gzip*)
		tar -xzf "$ipk" -C "$dest"
		;;
	*)
		(cd "$dest" && ar x "$ipk")
		;;
	esac
	if [ -f "$dest/data.tar.gz" ]; then
		tar -xzf "$dest/data.tar.gz" -C "$dest"
	elif [ -f "$dest/data.tar.zst" ]; then
		tar --zstd -xf "$dest/data.tar.zst" -C "$dest"
	else
		echo "missing data archive in $ipk" >&2
		exit 1
	fi
}

find_optional() {
	pattern="$1"
	for candidate in $root/bin/packages/*/*/$pattern; do
		[ -f "$candidate" ] || continue
		printf '%s\n' "$candidate"
		return 0
	done
	return 1
}

defaults_policy="$root/package/vendor/e87n-defaults/files/zz-e87n-network-policy"
defaults_makefile="$root/package/vendor/e87n-defaults/Makefile"
status_tool="$root/package/vendor/e87n-defaults/files/usr/sbin/e87n-offload-status"
turbo_defaults="$root/package/mtk/applications/luci-app-turboacc-mtk/root/etc/uci-defaults/turboacc"
turbo_makefile="$root/package/mtk/applications/luci-app-turboacc-mtk/Makefile"

grep -q "network.wan6.auto='0'" "$defaults_policy"
grep -q "fastpath_mh_eth_hnat_v6='0'" "$defaults_policy"
grep -q 'PKGARCH:=all' "$defaults_makefile"
grep -q 'zz-e87n-network-policy' "$defaults_makefile"
grep -q 'e87n-offload-status' "$defaults_makefile"
test -x "$status_tool"
grep -q 'fastpath_mh_eth_hnat_v6"="0"' "$turbo_defaults"
grep -q 'LUCI_PKGARCH:=all' "$turbo_makefile"
for required in luci-base rpcd kmod-mediatek_hnat luci-lua-runtime; do
	grep -q "+$required" "$turbo_makefile"
done
if grep -Eq 'luci-app-ttyd|kmod-fs-btrfs|kmod-tcp-bbr' "$turbo_makefile"; then
	echo 'TurboACC Makefile retained unrelated heavy dependencies' >&2
	exit 1
fi

defaults_ipk="$(find_optional 'e87n-defaults_*.ipk' || true)"
turbo_ipk="$(find_optional 'luci-app-turboacc-mtk_*.ipk' || true)"

if [ -z "$defaults_ipk" ] || [ -z "$turbo_ipk" ]; then
	echo 'E87N package source semantics passed (IPKs not present in this checkout)'
	exit 0
fi

extract_ipk "$defaults_ipk" "$tmp/defaults"
extract_ipk "$turbo_ipk" "$tmp/turbo"

grep -q "network.wan6.auto='0'" \
	"$tmp/defaults/etc/uci-defaults/zz-e87n-network-policy"
grep -q "fastpath_mh_eth_hnat_v6='0'" \
	"$tmp/defaults/etc/uci-defaults/zz-e87n-network-policy"
test -x "$tmp/defaults/usr/sbin/e87n-offload-status"
grep -q 'fastpath_mh_eth_hnat_v6"="0"' \
	"$tmp/turbo/etc/uci-defaults/turboacc"

if tar -xOf "$tmp/turbo/control.tar.gz" ./control 2>/dev/null | grep -Eq 'luci-app-ttyd|kmod-fs-btrfs|kmod-tcp-bbr'; then
	echo 'TurboACC package retained unrelated heavy dependencies' >&2
	exit 1
fi

echo 'E87N packaged policy checks passed'
