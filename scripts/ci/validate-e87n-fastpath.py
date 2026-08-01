#!/usr/bin/env python3
from pathlib import Path

root = Path('.')
device = (root / 'target/linux/mediatek/image/filogic.mk').read_text()
policy = (root / 'package/vendor/e87n-defaults/files/zz-e87n-network-policy').read_text()
status = (root / 'package/vendor/e87n-defaults/files/usr/sbin/e87n-offload-status').read_text()
turbo_defaults = (
    root / 'package/mtk/applications/luci-app-turboacc-mtk/root/etc/uci-defaults/turboacc'
).read_text()
turbo_service = (
    root / 'package/mtk/applications/luci-app-turboacc-mtk/root/etc/init.d/turboacc'
).read_text()
turbo_makefile = (
    root / 'package/mtk/applications/luci-app-turboacc-mtk/Makefile'
).read_text()

start = device.index('define Device/edgepi_e87n')
end = device.index('endef', start)
profile = device[start:end]
assert 'luci-app-turboacc-mtk' in profile
assert 'kmod-mediatek_hnat' in profile

required_policy = (
    "network.wan6.auto='0'",
    "dhcp.lan.ra='disabled'",
    "firewall.@defaults[0].flow_offloading='0'",
    "firewall.@defaults[0].flow_offloading_hw='0'",
    "turboacc.config.fastpath='mediatek_hnat'",
    "turboacc.config.fastpath_mh_eth_hnat='1'",
    "turboacc.config.fastpath_mh_eth_hnat_v6='0'",
)
for value in required_policy:
    assert value in policy, value

assert 'fastpath_mh_eth_hnat_v6"="0"' in turbo_defaults
assert 'fastpath_mh_eth_hnat_v6 "config" "fastpath_mh_eth_hnat_v6" "0"' in turbo_service
assert 'luci-app-ttyd' not in turbo_makefile
assert 'kmod-fs-btrfs' not in turbo_makefile
assert 'kmod-tcp-bbr' not in turbo_makefile

required_status = (
    'hnat_hook=',
    'nft_flowtable=',
    'ipv6_default_route=',
    '/sys/kernel/debug/hnat/hnat_stats',
    "printf 'eth1_%s='",
    "printf 'openclash='",
    "printf 'tun='",
)
for field in required_status:
    assert field in status, field

print('E87N fastpath policy checks passed')
