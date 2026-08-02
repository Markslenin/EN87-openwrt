#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
target="${1:-$root/bin/targets/mediatek/filogic}"
manifest="$target/immortalwrt-mediatek-filogic-edgepi_e87n.manifest"
config="$target/config.buildinfo"
build_config="${E87N_BUILD_CONFIG:-$root/.config}"

test -f "$manifest" || { echo "missing manifest: $manifest" >&2; exit 1; }
test -f "$config" || { echo "missing config.buildinfo: $config" >&2; exit 1; }
test -f "$build_config" || { echo "missing complete build config: $build_config" >&2; exit 1; }

has_package() { grep -Eq "^$1( - |$)" "$manifest"; }
for package in \
	kmod-cfg80211 kmod-mac80211 kmod-mt76-core kmod-mt76-connac \
	kmod-mt792x-common kmod-mt7921-common kmod-mt7921e \
	kmod-mt7921-firmware kmod-mt7922-firmware wireless-regdb wifi-scripts \
	wpad-openssl hostapd-utils wpa-cli iw-full iwinfo rpcd-mod-iwinfo \
	luci-mod-network luci-mod-status pciutils ethtool iperf3 tcpdump-mini; do
	has_package "$package" || { echo "manifest missing $package" >&2; exit 1; }
done
grep -Eq '^kmod-mt7921e - .+-r3$' "$manifest" || {
	echo 'manifest does not contain the txpower-backport mt7921e release' >&2
	exit 1
}
grep -Eq '^e87n-defaults - 1-r6$' "$manifest" || {
	echo 'manifest does not contain the HE160-default policy release' >&2
	exit 1
}
# libiwinfo is a virtual selection; the manifest records its ABI-versioned
# implementation (for example libiwinfo20230701), not the virtual name.
grep -Eq '^libiwinfo[0-9]+( - |$)' "$manifest" || {
	echo 'manifest missing ABI-versioned libiwinfo provider' >&2
	exit 1
}

providers="$(sed -nE '/^(wpad(-[^ ]+)?|hostapd(-basic|-full|-mini|-mbedtls|-openssl|-wolfssl)?|wpa-supplicant(-[^ ]+)?)($| - )/s/ .*//p' "$manifest" | sort -u)"
[ "$providers" = "wpad-openssl" ] || {
	echo "unexpected hostapd/wpa_supplicant providers: $providers" >&2
	exit 1
}
! has_package mt76-test
# config.buildinfo intentionally omits some global build feature gates. The
# generated .config is the authoritative record for these release assertions.
grep -q '^CONFIG_DRIVER_11AX_SUPPORT=y$' "$build_config"
grep -q '^CONFIG_PACKAGE_wpad-openssl=y$' "$build_config"
! grep -q '^CONFIG_PACKAGE_CFG80211_TESTMODE=y$' "$build_config"
! grep -q '^CONFIG_PACKAGE_mt76-test=y$' "$build_config"

rootfs="${E87N_ROOTFS:-}"
if [ -z "$rootfs" ]; then
	for candidate in "$root"/build_dir/target-*/root-mediatek; do
		[ -f "$candidate/etc/openwrt_release" ] || continue
		rootfs="$candidate"
		break
	done
fi
[ -n "$rootfs" ] && [ -d "$rootfs" ] || { echo 'E87N rootfs staging directory not found' >&2; exit 1; }

for module in mt76.ko mt76-connac-lib.ko mt792x-lib.ko mt7921-common.ko mt7921e.ko; do
	find "$rootfs/lib/modules" -type f -name "$module" -print -quit | grep -q . || {
		echo "rootfs missing module $module" >&2; exit 1;
	}
done
for firmware in \
	WIFI_MT7961_patch_mcu_1_2_hdr.bin WIFI_RAM_CODE_MT7961_1.bin \
	WIFI_MT7922_patch_mcu_1_1_hdr.bin WIFI_RAM_CODE_MT7922_1.bin; do
	test -f "$rootfs/lib/firmware/mediatek/$firmware" || {
		echo "rootfs missing firmware $firmware" >&2; exit 1;
	}
done
for file in \
	sbin/wifi usr/sbin/wpad usr/sbin/iw usr/bin/iwinfo usr/sbin/wpa_cli \
	usr/sbin/hostapd_cli usr/sbin/e87n-mt7922-status usr/sbin/e87n-wifi-profile \
	etc/init.d/e87n-mt7922-check etc/uci-defaults/96-e87n-mt7922-wireless \
	lib/netifd/wireless/mac80211.sh; do
	test -e "$rootfs/$file" || { echo "rootfs missing $file" >&2; exit 1; }
done
grep -Fq "set wireless.\$radio.htmode='HE160'" \
	"$rootfs/etc/uci-defaults/96-e87n-mt7922-wireless"
grep -Fq "set wireless.\$radio.txpower='30'" \
	"$rootfs/etc/uci-defaults/96-e87n-mt7922-wireless"
grep -q 'wireless.txpower_reported_dbm' "$rootfs/usr/sbin/e87n-mt7922-status"

echo 'E87N MT7922 image checks passed'
