#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_root"

required_files='
.github/workflows/e87n-release-build.yml
CONTRIBUTORS.md
feeds.conf.default
configs/e87n.config
configs/e87n-openclash.config
configs/e87n-openclash.example.yaml
package/vendor/e87n-defaults/Makefile
package/vendor/e87n-defaults/files/etc/crontabs/root
package/vendor/e87n-defaults/files/zz-e87n-network-policy
package/vendor/e87n-defaults/files/96-e87n-mt7922-wireless
package/vendor/e87n-defaults/files/etc/init.d/e87n-mt7922-check
package/vendor/e87n-defaults/files/usr/sbin/e87n-mt7922-status
package/vendor/e87n-defaults/files/usr/sbin/e87n-offload-status
package/vendor/e87n-defaults/files/usr/sbin/e87n-wifi-profile
package/kernel/mt76/Makefile
package/kernel/mt76/patches/001-wifi-mt76-mt7921-add-160-mhz-ap-for-mt7922.patch
package/kernel/mt76/patches/002-wifi-mt76-mt792x-report-txpower-for-vif.patch
package/vendor/display-control/Makefile
package/vendor/fancontrol/Makefile
package/vendor/openclash-core/Makefile
package/network/config/firewall4/patches/003-restore-configurable-flow-offload.patch
target/linux/mediatek/dts/mt7987a-edgepi-e87n.dts
target/linux/mediatek/image/filogic.mk
target/linux/mediatek/patches-6.6/999-fbtft-01-staging-fbtft-add-nv3007-driver.patch
scripts/ci/validate-e87n-fastpath.py
scripts/ci/validate-e87n-mt7922.py
scripts/ci/e87n-mt7922-image-check.sh
scripts/ci/e87n-package-check.sh
scripts/release/e87n-release-assets.sh
docs/hnat-validation.md'

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
grep -q 'luci-app-turboacc-mtk' target/linux/mediatek/image/filogic.mk
grep -q '^PKG_RELEASE:=6$' package/vendor/e87n-defaults/Makefile
grep -q '^PKG_RELEASE=3$' package/kernel/mt76/Makefile
grep -q 'OpenAI Codex' CONTRIBUTORS.md
grep -q 'e87n-release-build.yml' README.md
grep -q 'release-sha256sums' docs/development.md

if git ls-files | grep -Ei '(^|/)(google-vps[^/]*\.ya?ml|e87n-pre-[^/]*\.tar\.gz|etc/openclash/config/[^/]*\.ya?ml)$'; then
	echo 'deployment OpenClash configuration or backup is tracked' >&2
	exit 1
fi

for pin in \
	50afba57f43f57ed94e8c117c40a343cd9929126 \
	4936dfeddea460a4734fa4acdc68a9df1ace200c \
	946e9ff93be935fce6c03f4c02124833c35c2f56 \
	92892fa285360b8981f62bf4e0a097e6449e7e33; do
	grep -q "\\^$pin$" feeds.conf.default
done
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
if grep -RIl "$cr" scripts configs docs .github \
	package/vendor/display-control package/vendor/e87n-defaults \
	package/vendor/openclash-core/Makefile \
	--exclude='*.ttf' --exclude='display' --exclude='OFL.txt'; then
	echo 'CRLF found in E87N source files' >&2
	exit 1
fi
if grep -Il "$cr" README.md CONTRIBUTORS.md feeds.conf.default; then
	echo 'CRLF found in E87N root metadata' >&2
	exit 1
fi

python3 scripts/ci/validate-e87n-openclash.py
python3 scripts/ci/validate-e87n-fastpath.py
python3 scripts/ci/validate-e87n-mt7922.py
python3 - <<'PY'
from pathlib import Path

import yaml

for path in Path('.github/workflows').glob('*.yml'):
    with path.open(encoding='utf-8') as stream:
        document = yaml.safe_load(stream)
    if not isinstance(document, dict) or 'jobs' not in document:
        raise SystemExit(f'invalid workflow structure: {path}')
PY
sh -n package/vendor/e87n-defaults/files/zz-e87n-network-policy
sh -n package/vendor/e87n-defaults/files/96-e87n-mt7922-wireless
sh -n package/vendor/e87n-defaults/files/etc/init.d/e87n-mt7922-check
sh -n package/vendor/e87n-defaults/files/usr/sbin/e87n-mt7922-status
sh -n package/vendor/e87n-defaults/files/usr/sbin/e87n-offload-status
sh -n package/vendor/e87n-defaults/files/usr/sbin/e87n-wifi-profile
sh -n scripts/ci/e87n-package-check.sh
sh -n scripts/ci/e87n-mt7922-image-check.sh
sh -n scripts/release/e87n-release-assets.sh

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
	grep -q '^CONFIG_PACKAGE_luci-app-turboacc-mtk=y$' .config
	grep -q '^CONFIG_DRIVER_11AX_SUPPORT=y$' .config
	grep -q '^CONFIG_PACKAGE_kmod-mt7921e=y$' .config
	grep -q '^CONFIG_PACKAGE_kmod-mt7922-firmware=y$' .config
	grep -q '^CONFIG_PACKAGE_wpad-openssl=y$' .config
	! grep -q '^CONFIG_PACKAGE_CFG80211_TESTMODE=y$' .config
	! grep -q '^CONFIG_PACKAGE_mt76-test=y$' .config
	! grep -q '^CONFIG_PACKAGE_luci-app-ttyd=y$' .config

	cp configs/e87n-openclash.config .config
	make defconfig
	grep -q '^CONFIG_TARGET_mediatek_filogic_DEVICE_edgepi_e87n=y$' .config
	grep -q '^CONFIG_PACKAGE_luci-app-openclash=y$' .config
	grep -q '^CONFIG_PACKAGE_openclash-core=y$' .config
	grep -q '^CONFIG_PACKAGE_kmod-nft-tproxy=y$' .config
	grep -q '^CONFIG_PACKAGE_kmod-tun=y$' .config
	grep -q '^CONFIG_PACKAGE_kmod-mt7921e=y$' .config
	grep -q '^CONFIG_PACKAGE_wpad-openssl=y$' .config
	! grep -q '^CONFIG_PACKAGE_CFG80211_TESTMODE=y$' .config
	! grep -q '^CONFIG_PACKAGE_mt76-test=y$' .config
fi

echo 'E87N source checks passed'
