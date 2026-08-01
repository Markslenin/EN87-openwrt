# E87N MT7922 wireless subsystem

The populated E87N PCIe radio identifies as MediaTek `14c3:0616` and is served
by the upstream mt76 `mt7921e` driver. The pinned mt76 snapshot predates native
MT7922 AP HE160 advertisement, so the package carries the focused upstream
backport `8a24527e6c63914b838698ed78c44cb8a189129a`. It changes only the MT7922
AP HE capability initialization and leaves the PCIe DTS and kernel test-mode
policy unchanged.

The image intentionally uses the inherited custom wireless-regdb policy. For
CN, 5150-5350 MHz is published as one 160 MHz/30 dBm range without the upstream
`DFS` and `NO-OUTDOOR` flags. This behavior is explicit project policy and must
not be described as the upstream regulatory database.

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
it does not change the configured country or rewrite the active regulatory
database.

```sh
e87n-wifi-profile 5g-he80
e87n-wifi-profile 5g-he160
e87n-wifi-profile 6g-he80
```

HE160 remains on channel 36 and depends on both the MT7922 AP capability
backport and the active custom CN rule. The 6 GHz profile uses automatic
channel selection, WPA3-SAE and required PMF; availability depends on the
MT7922 AP capability and the active regulatory domain. Profile acceptance is
therefore a runtime result, not a build-time promise.

The v0.3.0 hardware-validation image was tested on the populated E87N. PCI ID
`14c3:0616` bound `mt7921e`, the AP capability block advertised `HE160/5GHz`,
and CN/channel 36 reached `AP-ENABLED` with `iw` reporting an actual 160 MHz
channel centered at 5250 MHz. HE80 remains the fresh-install default.

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
endpoint. HE160 acceptance additionally requires the AP capability block in
`iw phy` to advertise `HE160/5GHz`, hostapd to reach `AP-ENABLED`, and `iw dev`
to report an actual 160 MHz channel. HE160 and 6 GHz must be tested separately
and are not release defaults.

Release configurations must keep `CFG80211_TESTMODE` and `mt76-test` disabled.
