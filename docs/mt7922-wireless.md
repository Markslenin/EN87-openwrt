# E87N MT7922 wireless subsystem

The populated E87N PCIe radio identifies as MediaTek `14c3:0616` and is served
by the upstream mt76 `mt7921e` driver. This integration does not change the
PCIe DTS, wireless-regdb, DFS behavior, or kernel test-mode policy.

## Default policy

A fresh configuration runs `/sbin/wifi config` and claims only an untouched,
disabled mac80211 AP attached to PCI path `0000:01:00.0`. It creates:

- 5 GHz, channel 36, HE80;
- SSID `E87N-5G` on `lan`;
- WPA2/WPA3 mixed authentication with optional PMF;
- a random per-device 20-character key in `/etc/e87n/wifi-default-key`.

The defaults script leaves an existing enabled, encrypted, or keyed wireless
configuration unchanged. The key file is local runtime state and must never be
committed to the repository or copied into release metadata.

## Optional profiles

The profile helper changes only band/channel/width and the authentication mode;
it does not override the configured country or bypass regulatory decisions.

```sh
e87n-wifi-profile 5g-he80
e87n-wifi-profile 5g-he160
e87n-wifi-profile 6g-he80
```

HE160 remains on channel 36 and is subject to the current country/regdb/DFS
combination. The 6 GHz profile uses automatic channel selection, WPA3-SAE and
required PMF; availability depends on the MT7922 AP capability and the active
regulatory domain. Profile acceptance is therefore a runtime result, not a
build-time promise.

## Image acceptance

The formal build runs `scripts/ci/e87n-mt7922-image-check.sh`. It checks the
manifest, 802.11ax gate, unique `wpad-openssl` provider, disabled test packages,
driver modules, four firmware files, wifi-scripts, CLI tools, LuCI dependencies
and the E87N runtime helpers in the staged root filesystem.

## Post-flash acceptance

After flashing a candidate image, run:

```sh
e87n-mt7922-status --wait 90
lspci -nnk -s 0000:01:00.0
iw phy
iwinfo
logread | grep -E 'mt7921e|mt7922|firmware|e87n-mt7922'
```

Acceptance requires PCI ID `14c3:0616`, driver `mt7921e`, all dependency
modules loaded, all four firmware files present, and a PHY linked to the PCIe
endpoint. Only then should the HE80 AP be functionally tested. HE160 and 6 GHz
must be tested separately and are not release defaults.

Release configurations must keep `CFG80211_TESTMODE` and `mt76-test` disabled.
