# E87N build profile

This document is the policy for the default `edgepi_e87n` image. Package names
live in `Device/edgepi_e87n`; `configs/e87n.config` only selects the target and
image formats so `make defconfig` remains reproducible.

## Included by default

### Board and networking

- EdgePi E87N DTS, eMMC sysupgrade, F2FS overlay and port mapping.
- MediaTek HNAT/WED support and Safexcel crypto acceleration.
- TurboACC configured for the MediaTek HNAT engine, with IPv6 HNAT disabled.
- `e87n-defaults` release 5 carries the current first-boot network/wireless policy and
  acceleration diagnostic script; bump its package release whenever installed
  file content changes.
- MT7987 internal 2.5G PHY driver and PMB/DSP firmware.
- IPv6 kernel support remains available, but the E87N first-boot policy disables
  WAN6 autostart and LAN router advertisements to prevent proxy bypass.
- When sysupgrade moves from the optional OpenClash image to the stable image,
  the defaults remove only the exact stale `127.0.0.1#7874` dnsmasq endpoint;
  unrelated administrator DNS settings remain untouched.
- Firewall4, `dnsmasq-full`, PPPoE and IPv6 client/router services inherited
  from the ImmortalWrt target defaults where not overridden above.

### Management and secure access

- LuCI with Simplified Chinese and the Bootstrap fallback theme.
- Aurora theme, vendored at a reviewed and pinned upstream commit.
- `luci-ssl-openssl` plus an idempotent board default that enables IPv4 and
  IPv6 HTTPS listeners without replacing an administrator's existing setting.
- WireGuard kernel support, `wireguard-tools` and LuCI protocol integration.

### MT7922 wireless

- cfg80211/mac80211 with the 802.11ac and 802.11ax build gates enabled.
- The complete mt76 PCIe dependency chain: mt76 core, connac, mt792x common,
  mt7921 common and mt7921e.
- MT7921 and MT7922 firmware plus `wireless-regdb`. The inherited
  `600-custom-change-txpower-and-dfs.patch` is intentionally active: for CN it
  publishes 5150-5350 MHz at up to 160 MHz/30 dBm without the upstream
  `DFS`/`NO-OUTDOOR` flags. This is a project policy, not an upstream default.
- The pinned February 2025 mt76 snapshot carries the focused upstream backport
  `8a24527e6c63914b838698ed78c44cb8a189129a`, enabling 160 MHz capability for
  MT7922 AP interfaces without updating the rest of mt76.
- `wifi-scripts`, netifd/ucode dependencies, LuCI wireless configuration and
  status support, `iw-full`, iwinfo and rpcd iwinfo integration.
- Exactly one combined AP/supplicant provider: `wpad-openssl`, accompanied by
  `hostapd-utils` and `wpa-cli`.
- A fresh installation creates a secure `E87N-5G` AP on channel 36 HE80. The
  generated key is unique to the device and is stored mode 0600 at
  `/etc/e87n/wifi-default-key`; preserved user configuration is not replaced.
- HE160 and 6 GHz remain opt-in profiles. HE160 requires separate live RF
  acceptance; see `docs/mt7922-wireless.md`.

### Storage and diagnostics

- USB mass storage and UAS.
- Ext4, FAT/VFAT, exFAT and read/write NTFS3 kernel filesystems.
- Filesystem tools for Ext4, FAT and exFAT.
- `curl`, `htop`, `jq`, `tcpdump-mini` and `iperf3` for practical diagnosis.

### E87N peripherals

- PWM fan supervision and honest no-tachometer reporting.
- NV3007 framebuffer, PWM backlight, native dashboard and vendor fallback.
- USB serial and common USB modem transport drivers. No modem management
  application is selected without a confirmed modem population.

## Intentionally optional

The following are not defects in the default image:

- **OpenClash and Mihomo:** policy-specific and large, so they remain outside
  the hardware baseline. A reproducible optional image is provided through
  `configs/e87n-openclash.config`; see `docs/openclash.md`.
- **SQM:** software queueing usually bypasses or conflicts with the HNAT path;
  enable it only for a measured shaping requirement.
- **UPnP:** not enabled by default because it permits LAN clients to create
  inbound mappings.
- **TTYD/web terminal:** omitted to avoid exposing a browser-accessible root
  shell by default.
- **Docker:** large runtime and storage footprint; install only with a planned
  container data volume.
- **Samba/KSMBD:** sharing policy, users and storage mounts need explicit
  configuration; filesystem support is present first.
- **DDNS:** provider credentials and hostname policy are deployment-specific.
- **Ad blocking, VPN servers and traffic statistics:** useful but policy- and
  workload-specific rather than safe universal defaults.

## Review rule

A package belongs in the default image only when it is useful on most E87N
deployments, has a maintained source, does not assume unverified hardware, and
does not silently disable hardware acceleration or expose a new privileged
network service. Everything else should remain installable through packages or
a separate deployment profile.
