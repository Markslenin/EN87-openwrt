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

find_one() {
	pattern="$1"
	set -- $root/bin/packages/*/*/$pattern
	[ -f "$1" ] || {
		echo "package not built: $pattern" >&2
		exit 1
	}
	printf '%s\n' "$1"
}

firewall_ipk="$(find_one 'firewall4_*.ipk')"
defaults_ipk="$(find_one 'e87n-defaults_*.ipk')"
turbo_ipk="$(find_one 'luci-app-turboacc-mtk_*.ipk')"

extract_ipk "$firewall_ipk" "$tmp/firewall"
extract_ipk "$defaults_ipk" "$tmp/defaults"
extract_ipk "$turbo_ipk" "$tmp/turbo"

grep -q 'flags offload;' "$tmp/firewall/usr/share/firewall4/templates/ruleset.uc"
grep -q 'if (!this.default_option("flow_offloading"))' \
	"$tmp/firewall/usr/share/ucode/fw4.uc"
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
