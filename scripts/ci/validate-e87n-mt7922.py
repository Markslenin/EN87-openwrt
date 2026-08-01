#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]

required_packages = {
    "kmod-cfg80211", "kmod-mac80211", "kmod-mt76-core", "kmod-mt76-connac",
    "kmod-mt792x-common", "kmod-mt7921-common", "kmod-mt7921e",
    "kmod-mt7921-firmware", "kmod-mt7922-firmware", "wireless-regdb",
    "wifi-scripts", "wpad-openssl", "hostapd-utils", "wpa-cli", "iw-full",
    "iwinfo", "libiwinfo", "rpcd-mod-iwinfo", "luci-mod-network",
    "luci-mod-status", "pciutils", "ethtool", "iperf3", "tcpdump-mini",
}

for config_name in ("e87n.config", "e87n-openclash.config"):
    text = (root / "configs" / config_name).read_text(encoding="utf-8")
    for gate in ("CONFIG_DRIVER_11AC_SUPPORT=y", "CONFIG_DRIVER_11AX_SUPPORT=y"):
        assert gate in text, f"{config_name}: missing {gate}"
    for package in required_packages:
        assert f"CONFIG_PACKAGE_{package}=y" in text, f"{config_name}: missing {package}"
    assert "# CONFIG_PACKAGE_CFG80211_TESTMODE is not set" in text
    assert "# CONFIG_PACKAGE_mt76-test is not set" in text

image = (root / "target/linux/mediatek/image/filogic.mk").read_text(encoding="utf-8")
match = re.search(r"define Device/edgepi_e87n\n(.*?)\nendef", image, re.S)
assert match, "missing edgepi_e87n image definition"
block = match.group(1)
package_block = block.split("\n\t# Keep", 1)[0]
positive = {word for word in re.findall(r"(?<!-)\b[\w.+-]+\b", package_block)}
for package in required_packages:
    assert package in positive, f"image: missing positive package {package}"

provider_pattern = re.compile(
    r"^(?:wpad(?:-.+)?|hostapd(?:-(?:basic.*|full|mini|mbedtls|openssl|wolfssl))?"
    r"|wpa-supplicant(?:-.+)?)$"
)
providers = {package for package in positive if provider_pattern.match(package)}
assert providers == {"wpad-openssl"}, f"image provider set is {sorted(providers)}"
assert "mt76-test" not in positive

mt76 = (root / "package/kernel/mt76/Makefile").read_text(encoding="utf-8")
for definition in (
    "KernelPackage/mt76-core", "KernelPackage/mt76-connac",
    "KernelPackage/mt792x-common", "KernelPackage/mt7921-common",
    "KernelPackage/mt7921e", "KernelPackage/mt7921-firmware",
    "KernelPackage/mt7922-firmware",
):
    assert f"define {definition}" in mt76, f"mt76: missing {definition}"
for firmware in (
    "WIFI_MT7961_patch_mcu_1_2_hdr.bin", "WIFI_RAM_CODE_MT7961_1.bin",
    "WIFI_MT7922_patch_mcu_1_1_hdr.bin", "WIFI_RAM_CODE_MT7922_1.bin",
):
    assert firmware in mt76, f"mt76: missing firmware {firmware}"

defaults = (root / "package/vendor/e87n-defaults/files/96-e87n-mt7922-wireless").read_text()
for setting in ("channel='36'", "htmode='HE80'", "encryption='sae-mixed'", "country='CN'"):
    assert setting in defaults, f"default AP: missing {setting}"
assert "/dev/urandom" in defaults and "wifi-default-key" in defaults

profiles = (root / "package/vendor/e87n-defaults/files/usr/sbin/e87n-wifi-profile").read_text()
for profile in ("5g-he80", "5g-he160", "6g-he80"):
    assert profile in profiles, f"profile tool: missing {profile}"
assert "set wireless.$radio.country" not in profiles, "profiles must not override country"
assert not re.search(r"(dfs|regdb).*(disable|bypass|ignore)", profiles, re.I)

print("E87N MT7922 source semantics passed")
