#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_root"

required_files='
configs/e87n.config
configs/e87n-openclash.config
configs/e87n-openclash.example.yaml
package/vendor/e87n-defaults/Makefile
package/vendor/e87n-defaults/files/etc/crontabs/root
package/vendor/display-control/Makefile
package/vendor/fancontrol/Makefile
package/vendor/openclash-core/Makefile
package/network/config/firewall4/patches/003-restore-configurable-flow-offload.patch
target/linux/mediatek/dts/mt7987a-edgepi-e87n.dts
target/linux/mediatek/image/filogic.mk
target/linux/mediatek/patches-6.6/999-fbtft-01-staging-fbtft-add-nv3007-driver.patch'

for path in $required_files; do
	test -e "$path" || {
		echo "missing required E87N file: $path" >&2
		exit 1
	}
done

if grep -RnsE 'HiGoROS-E87N|immortalwrt-mt798x-6\.6' \
	README.md docs configs package/vendor target/linux/mediatek/image \
	--exclude='e87n-check.sh'; then
	echo 'obsolete project name or wrapper path found' >&2
	exit 1
fi

grep -q 'BOARD_NAME := edgepi,e87n' target/linux/mediatek/image/filogic.mk
grep -q 'e87n-defaults' target/linux/mediatek/image/filogic.mk
grep -q 'FBTFT_REGISTER_SPI_DRIVER(DRVNAME, "newvisionu", "nv3007"' \
	target/linux/mediatek/patches-6.6/999-fbtft-01-staging-fbtft-add-nv3007-driver.patch
grep -q '^#define ANIMATION_FPS 30$' package/vendor/display-control/src/display-e87n.c
grep -q '^#define DASHBOARD_FPS 3$' package/vendor/display-control/src/display-e87n.c
grep -q 'INSTALL_CONF.*files/etc/crontabs/root' package/vendor/e87n-defaults/Makefile
grep -q 'if (!this.default_option("flow_offloading"))' \
	package/network/config/firewall4/patches/003-restore-configurable-flow-offload.patch
grep -q 'flags offload;' \
	package/network/config/firewall4/patches/003-restore-configurable-flow-offload.patch
if grep -RnsE 'resolve_offload_devices: function\(\).*return \[\];' \
	package/network/config/firewall4/patches --include='*.patch'; then
	echo 'firewall4 flow offload is unconditionally disabled' >&2
	exit 1
fi

cr="$(printf '\r')"
if grep -RIl "$cr" scripts configs docs \
	package/vendor/display-control package/vendor/e87n-defaults \
	package/vendor/openclash-core/Makefile \
	--exclude='*.ttf' --exclude='display' --exclude='OFL.txt'; then
	echo 'CRLF found in E87N source files' >&2
	exit 1
fi

python3 scripts/ci/validate-e87n-openclash.py

openclash_root='feeds/luci/applications/luci-app-openclash'
grep -q "option operation_mode 'fake-ip'" "$openclash_root/root/etc/config/openclash"
grep -q 'o:value("fake-ip-tun"' "$openclash_root/luasrc/model/cbi/openclash/settings.lua"
grep -q 'if \[ "$en_mode" = "fake-ip-tun" \]' "$openclash_root/root/etc/init.d/openclash"

if [ "${E87N_SKIP_DEFCONFIG:-0}" != 1 ]; then
	cp configs/e87n.config .config
	make defconfig
	grep -q '^CONFIG_TARGET_mediatek_filogic_DEVICE_edgepi_e87n=y$' .config
	grep -q '^CONFIG_PACKAGE_e87n-defaults=y$' .config
	grep -q '^CONFIG_PACKAGE_display-control=y$' .config
	grep -q '^CONFIG_PACKAGE_fancontrol=y$' .config

	cp configs/e87n-openclash.config .config
	make defconfig
	grep -q '^CONFIG_TARGET_mediatek_filogic_DEVICE_edgepi_e87n=y$' .config
	grep -q '^CONFIG_PACKAGE_luci-app-openclash=y$' .config
	grep -q '^CONFIG_PACKAGE_openclash-core=y$' .config
	grep -q '^CONFIG_PACKAGE_kmod-nft-tproxy=y$' .config
	grep -q '^CONFIG_PACKAGE_kmod-tun=y$' .config
fi

echo 'E87N source checks passed'
