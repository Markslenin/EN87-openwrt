# EN87 OpenWrt

Complete ImmortalWrt/OpenWrt source tree for the EdgePi E87N router platform based on MediaTek MT7987A and Linux 6.6.

This repository is self-contained. It is not an outer build wrapper and does not use an OpenWrt source submodule.

## E87N baseline

- Board: EdgePi E87N (`edgepi,e87n`)
- SoC: MediaTek MT7987A
- Storage: eMMC with squashfs/F2FS persistent overlay and sysupgrade support
- Network: E87N port mapping, MediaTek HNAT packages, and packaged MT7987 internal 2.5G PHY firmware
- Cooling: PWM fan service with explicit reporting when no tachometer input is available
- Display: NewVision NV3007 142x428 SPI framebuffer driver, PWM backlight, and configurable dashboard service
- Management: LuCI with Simplified Chinese, Aurora theme, HTTPS, WireGuard UI, USB storage support, and practical diagnostics
- Base: `mt798x-mt799x-6.6-mtwifi` at `30fbc1d6deba23c0e850185021e9ee42214925eb`
- E87N baseline tip: `627a548c276d8e18b70a3e4faf7af9fac53793f3`
- Last deployed validation baseline: `r33551-a331bbaa3b`

The E87N image intentionally does not inherit the H5000M Wi-Fi 7 or modem package stacks until the corresponding E87N hardware population is confirmed.

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

## Repository layout

| Path | Purpose |
| --- | --- |
| `configs/e87n.config` | Reproducible E87N build configuration |
| `docs/e87n-build-profile.md` | Included and intentionally excluded default features |
| `docs/development.md` | Branch, source-check and release workflow |
| `docs/vendor-components.md` | License boundary and hashes for retained vendor assets |
| `scripts/ci/e87n-check.sh` | Local and CI source/profile validation |
| `target/linux/mediatek/dts/mt7987a-edgepi-e87n.dts` | Board hardware description |
| `target/linux/mediatek/image/filogic.mk` | E87N image and package profile |
| `target/linux/mediatek/patches-6.6/999-fbtft-01-staging-fbtft-add-nv3007-driver.patch` | NV3007 framebuffer driver |
| `package/vendor/fancontrol/` | Vendor fan daemon and E87N runtime integration |
| `package/vendor/display-control/` | Procd-managed native display service and vendor compatibility fallback |
| `package/vendor/e87n-defaults/` | Idempotent E87N first-boot defaults, including HTTPS listeners |
| `package/vendor/luci-theme-aurora/` | Pinned Aurora LuCI theme source snapshot |
| `package/mtk/applications/mt798x-2p5g-phy-firmware-internal/` | MT7987/MT7988 internal 2.5G PHY firmware packaging |

## Build validation

The `a331bbaa3b` source baseline completed `make -j32` with exit code 0 on Google Compute Engine, Ubuntu 24.04 x86_64, on 2026-08-01. It produced both `immortalwrt-mediatek-filogic-edgepi_e87n-initramfs-kernel.bin` and `immortalwrt-mediatek-filogic-edgepi_e87n-squashfs-sysupgrade.bin`.

The generated squashfs was inspected and contains the MT7987 PMB/DSP 2.5G PHY firmware, `fb_nv3007.ko`, `fancontrol`, `display-control`, and the native `display-e87n` binary. The sysupgrade control record identifies `BOARD=edgepi,e87n`; all generated entries in `sha256sums` passed verification.

That image was also installed on an E87N and verified to boot as
`r33551-a331bbaa3b` with a writable F2FS overlay. The board identity, Ethernet
port mapping and link state, packaged MT7987 2.5G PHY firmware, LuCI/Aurora,
HTTPS after configuration, fan service, framebuffer node, backlight and display
service were observed on the running system.

## Validation boundary

A successful source build verifies configuration, patch application, package dependencies, and image generation. The deployed validation above does not yet prove recovery-mode operation, pixel-perfect display output, fan tachometer sensing, long-duration PHY stability, hardware-offload effectiveness, or routed throughput.

The fan integration intentionally does not report a fabricated RPM value when the hardware exposes no validated tachometer input.

## Upstream and license

The source tree is derived from ImmortalWrt/OpenWrt and retains the licenses of the corresponding upstream components. Aurora is vendored from `eamonxg/luci-theme-aurora` at commit `e681ecb0f44ee3e1d5712b8104a3ad38a3e7c4da` under Apache-2.0. The native E87N renderer and integration scripts are GPL-2.0-or-later. Retained vendor fallback binaries remain proprietary; the bundled Oswald font is covered by the included OFL-1.1 license file. Exact component boundaries and hashes are recorded in `docs/vendor-components.md`.
