# E87N build profile

This document is the policy for the default `edgepi_e87n` image. Package names
live in `Device/edgepi_e87n`; `configs/e87n.config` only selects the target and
image formats so `make defconfig` remains reproducible.

## Included by default

### Board and networking

- EdgePi E87N DTS, eMMC sysupgrade, F2FS overlay and port mapping.
- MediaTek HNAT/WED support and Safexcel crypto acceleration.
- TurboACC configured for the MediaTek HNAT engine, with IPv6 HNAT disabled.
- MT7987 internal 2.5G PHY driver and PMB/DSP firmware.
- IPv6 kernel support remains available, but the E87N first-boot policy disables
  WAN6 autostart and LAN router advertisements to prevent proxy bypass.
- Firewall4, `dnsmasq-full`, PPPoE and IPv6 client/router services inherited
  from the ImmortalWrt target defaults where not overridden above.

### Management and secure access

- LuCI with Simplified Chinese and the Bootstrap fallback theme.
- Aurora theme, vendored at a reviewed and pinned upstream commit.
- `luci-ssl-openssl` plus an idempotent board default that enables IPv4 and
  IPv6 HTTPS listeners without replacing an administrator's existing setting.
- WireGuard kernel support, `wireguard-tools` and LuCI protocol integration.

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

- **Wi-Fi drivers and AP stack:** E87N wireless hardware population is not yet
  established for a stable baseline. `wpad` remains explicitly removed.
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
