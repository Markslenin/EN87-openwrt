# EN87 OpenWrt

Complete ImmortalWrt/OpenWrt source tree for the EdgePi E87N router platform based on MediaTek MT7987A and Linux 6.6.

This repository is self-contained. It is not an outer build wrapper and does not use an OpenWrt source submodule.

## E87N baseline

- Board: EdgePi E87N (`edgepi,e87n`)
- SoC: MediaTek MT7987A
- Storage: eMMC with squashfs/F2FS persistent overlay and sysupgrade support
- Network: E87N port mapping, MediaTek HNAT packages, and packaged MT7987 internal 2.5G PHY firmware
- Wireless: PCIe MT7922 (`14c3:0616`) through upstream cfg80211/mac80211 and mt76/mt7921e, with a secure 5 GHz HE80 first-boot AP
- Cooling: PWM fan service with explicit reporting when no tachometer input is available
- Display: NewVision NV3007 142x428 SPI framebuffer driver, PWM backlight, and configurable dashboard service
- Management: LuCI with Simplified Chinese, Aurora theme, HTTPS, WireGuard UI, USB storage support, and practical diagnostics
- Base: `mt798x-mt799x-6.6-mtwifi` at `30fbc1d6deba23c0e850185021e9ee42214925eb`
- Imported E87N baseline tip: `627a548c276d8e18b70a3e4faf7af9fac53793f3`
- Frozen hardware baseline: `v0.1.0` at `81d18efe3c7faf920a15823c84c5f2942d13efc5`
- Current release: `v0.3.0`
- Previous stable release: `v0.2.0` at `7942bcad75a449da847339a221d4c0281c74dc5e`
- v0.3.0 hardware-validation commit: `085af11b52b4e3108a1d94e5d03ce6edf8927abe`
- Last full-device firmware validation: `r0-085af11`, including MT7922 CN/HE160 AP operation

The E87N image intentionally does not inherit the H5000M Wi-Fi 7 or modem
package stacks. Its populated MT7922 uses the upstream mt76 driver family.

## Build

Use a case-sensitive Linux filesystem and build as an unprivileged user. Keep the checkout under the Linux home directory rather than a Windows-mounted WSL path.

```bash
git clone https://github.com/Markslenin/EN87-openwrt.git
cd EN87-openwrt

./scripts/feeds update -a
./scripts/feeds install -a

cp configs/e87n.config .config
make defconfig
make -j"$(nproc)"
```

Firmware artifacts are written below `bin/targets/mediatek/filogic/`. The E87N profile builds initramfs and squashfs/sysupgrade images.
The four feeds in `feeds.conf.default` are pinned to reviewed commits, so the
same source commit does not silently consume newer feed packages.

## Repository layout

| Path | Purpose |
| --- | --- |
| `configs/e87n.config` | Reproducible E87N build configuration |
| `configs/e87n-openclash.config` | Optional E87N image with OpenClash and a pinned Mihomo core |
| `docs/e87n-build-profile.md` | Included and intentionally excluded default features |
| `docs/mt7922-wireless.md` | MT7922 package closure, profiles and runtime acceptance checks |
| `docs/development.md` | Branch, source-check and release workflow |
| `docs/openclash.md` | Safe OpenClash build and first-configuration workflow |
| `docs/vendor-components.md` | License boundary and hashes for retained vendor assets |
| `CONTRIBUTORS.md` | Project ownership and implementation acknowledgements |
| `scripts/ci/e87n-check.sh` | Local and CI source/profile validation |
| `scripts/release/e87n-release-assets.sh` | Release-asset checksum generator |
| `.github/workflows/e87n-release-build.yml` | Manual/tag-gated full firmware build |
| `target/linux/mediatek/dts/mt7987a-edgepi-e87n.dts` | Board hardware description |
| `target/linux/mediatek/image/filogic.mk` | E87N image and package profile |
| `target/linux/mediatek/patches-6.6/999-fbtft-01-staging-fbtft-add-nv3007-driver.patch` | NV3007 framebuffer driver |
| `package/vendor/fancontrol/` | Vendor fan daemon and E87N runtime integration |
| `package/vendor/display-control/` | Procd-managed native display service and vendor compatibility fallback |
| `package/vendor/e87n-defaults/` | Idempotent E87N first-boot defaults, including HTTPS and secure MT7922 AP setup |
| `package/vendor/openclash-core/` | Hash-pinned official ARM64 Mihomo core for the optional profile |
| `package/vendor/luci-theme-aurora/` | Pinned Aurora LuCI theme source snapshot |
| `package/mtk/applications/mt798x-2p5g-phy-firmware-internal/` | MT7987/MT7988 internal 2.5G PHY firmware packaging |

## Build validation

The `v0.3.0` OpenClash profile was rebuilt from clean hardware-validation commit
`085af11b52b4e3108a1d94e5d03ce6edf8927abe` on Google Compute Engine with
Ubuntu 24.04 x86_64. The build completed with a clean worktree before and after,
produced initramfs and squashfs/sysupgrade images, and passed every generated
SHA-256 check. The manifest confirms the complete MT7922 dependency closure,
mt76 package revision `-r2`, a single `wpad-openssl` provider and no wireless
test-mode package. Release assets include the manifest, profile metadata and
build evidence alongside the firmware.

The currently deployed E87N boots as `r0-085af11` with a writable F2FS
overlay. Board identity, Ethernet mapping (`eth1` WAN and `eth0` LAN), packaged
MT7987 PHY firmware, LuCI/Aurora, HTTPS, fan, framebuffer, backlight, display,
OpenClash packaging and retained private configuration have been observed live.
The PCIe MT7922 (`14c3:0616`) binds `mt7921e`; its AP capability advertises
`HE160/5GHz`, and a CN/channel 36 AP reached `AP-ENABLED` at an actual 160 MHz
width. The prior MediaTek HNAT policy completed a 2 GiB direct-flow test at
about 538 Mbit/s with PPE bindings and no RX error or overflow increase. See
`docs/hnat-validation.md` for the acceptance boundary.

## Validation boundary

A successful source build verifies configuration, patch application, package
dependencies, image contents, and image generation. It does not prove that the
MT7922 binds, registers a PHY, or sustains RF traffic on the physical board;
those are explicit post-flash gates in `docs/mt7922-wireless.md`. Direct IPv4
PPE binding is validated for the tested policy, but this does not prove
proxy/TUN offload or universal throughput improvement. Recovery-mode operation,
pixel-perfect display output, fan tachometer sensing and long-duration PHY
stability remain outside the verified boundary.

The fan integration intentionally does not report a fabricated RPM value when the hardware exposes no validated tachometer input.

## Contributors

Project ownership and implementation acknowledgements are recorded in
[`CONTRIBUTORS.md`](CONTRIBUTORS.md).

## Upstream and license

The source tree is derived from ImmortalWrt/OpenWrt and retains the licenses of the corresponding upstream components. Aurora is vendored from `eamonxg/luci-theme-aurora` at commit `e681ecb0f44ee3e1d5712b8104a3ad38a3e7c4da` under Apache-2.0. The native E87N renderer and integration scripts are GPL-2.0-or-later. Retained vendor fallback binaries remain proprietary; the bundled Oswald font is covered by the included OFL-1.1 license file. Exact component boundaries and hashes are recorded in `docs/vendor-components.md`.
