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
- Base: `mt798x-mt799x-6.6-mtwifi` at `30fbc1d6deba23c0e850185021e9ee42214925eb`
- E87N baseline tip: `627a548c276d8e18b70a3e4faf7af9fac53793f3`

The E87N image intentionally does not inherit the H5000M Wi-Fi 7 or modem package stacks until the corresponding E87N hardware population is confirmed.

## Build

Use a case-sensitive Linux filesystem and build as an unprivileged user. Keep the checkout under the Linux home directory rather than a Windows-mounted WSL path.

```bash
git clone https://github.com/Markslenin/EN87-openwrt.git
cd EN87-openwrt

./scripts/feeds update -a
./scripts/feeds install -a

cp configs/e87n-fan-display.config .config
make defconfig
make -j"$(nproc)"
```

Firmware artifacts are written below `bin/targets/mediatek/filogic/`. The E87N profile builds initramfs and squashfs/sysupgrade images.

## Repository layout

| Path | Purpose |
| --- | --- |
| `configs/e87n-fan-display.config` | Reproducible E87N build configuration |
| `target/linux/mediatek/dts/mt7987a-edgepi-e87n.dts` | Board hardware description |
| `target/linux/mediatek/image/filogic.mk` | E87N image and package profile |
| `target/linux/mediatek/patches-6.6/999-fbtft-01-staging-fbtft-add-nv3007-driver.patch` | NV3007 framebuffer driver |
| `package/vendor/fancontrol/` | Vendor fan daemon and E87N runtime integration |
| `package/vendor/display-control/` | Native display service and vendor compatibility fallback |
| `package/mtk/applications/mt798x-2p5g-phy-firmware-internal/` | MT7987/MT7988 internal 2.5G PHY firmware packaging |

## Validation boundary

A successful source build verifies configuration, patch application, package dependencies, and image generation. It does not by itself verify boot, eMMC upgrade/recovery, fan electrical behavior, display operation, PHY link stability, hardware offload, or routed throughput on a physical E87N board.

The fan integration intentionally does not report a fabricated RPM value when the hardware exposes no validated tachometer input.

## Upstream and license

The source tree is derived from ImmortalWrt/OpenWrt and retains the licenses of the corresponding upstream components. Vendor binaries under `package/vendor/` remain proprietary; the bundled Oswald font is covered by the included OFL-1.1 license file.
