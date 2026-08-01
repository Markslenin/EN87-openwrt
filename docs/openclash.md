# E87N OpenClash profile

OpenClash is an optional policy profile and is not part of the frozen v0.1.0
hardware baseline. Build it with:

```sh
cp configs/e87n-openclash.config .config
make defconfig
make -j"$(nproc)"
```

## Reproducible core

The profile selects `luci-app-openclash` from the LuCI feed and the local
`openclash-core` package. The latter installs the official ARM64 Mihomo binary
at `/etc/openclash/core/clash_meta`.

The LuCI feed is locked at
`4936dfeddea460a4734fa4acdc68a9df1ace200c`; updating that pin is the explicit
review point for future OpenClash package changes.

- Mihomo version: `1.19.29`
- Upstream asset: `mihomo-linux-arm64-v1.19.29.gz`
- SHA-256: `9a868b5e4e0ad91d9d71e1b41b0cfce78aaba44360c30df74a723f8e3926a86c`

The OpenClash service remains disabled in a fresh image. Its feed defaults use
`fake-ip` mode, disable automatic core updates, and listen on the LAN dashboard
port only after the service is configured. No subscription, node, token,
dashboard secret or provider credential belongs in this repository.

`configs/e87n-openclash.example.yaml` is a credential-free, CI-validated
starting point for the E87N. It keeps DNS and policy inside Mihomo while
OpenClash owns TUN creation, DNS interception, firewall rules and LuCI:

- Fake-IP DNS uses direct AliDNS and DNSPod DoH without an overseas fallback.
- Six MetaCubeX MRS providers refresh every 24 hours through the proxy node.
- The `PROXY` group is fail-closed and never falls back to `DIRECT`.
- TUN and sniffer blocks are intentionally absent from the YAML template.

The example contains a TEST-NET address and a non-production test key. Replace
both locally; never commit a working node or subscription.

## First configuration

1. Flash the OpenClash-profile image or install both generated packages.
2. Import a local Clash/Mihomo configuration or add a subscription in LuCI.
3. Set a dashboard secret before exposing the controller to LAN clients.
4. For TUN deployments select `fake-ip-tun`. OpenClash keeps
   `operation_mode=fake-ip` as the page's DNS operation mode; this is expected
   and does not mean TUN is disabled.
5. Enable OpenClash and verify DNS, the controller API, the `utun` route and an
   explicit proxied request.
6. Benchmark direct and proxied throughput separately before changing HNAT,
   firewall offload or interface mapping.

Do not assume that HNAT/PPE accelerates traffic processed through Mihomo TUN or
TProxy. Back up `/etc/config/openclash` and `/etc/openclash/` before mode,
firewall or DNS changes.

## Validated direct-flow acceleration

The E87N contains two distinct acceleration controls: firewall4's generic
nftables flowtable and MediaTek's proprietary HNAT hook. LuCI's software and
hardware flow-offload checkboxes control only the former. They do not report
the HNAT hook state.

The validated live-router combination is:

- `openclash.config.china_ip_route=1`
- `firewall.@defaults[0].flow_offloading=0`
- `firewall.@defaults[0].flow_offloading_hw=0`
- `turboacc.config.fastpath=mediatek_hnat`
- `turboacc.config.fastpath_mh_eth_hnat=1`
- `turboacc.config.fastpath_mh_eth_hnat_v6=0`
- `/sys/kernel/debug/hnat/hook_toggle` reports `enabled`

With this combination, domestic IPv4 traffic in OpenClash's China route set
bypasses Mihomo TUN and can bind to the MediaTek PPE. Overseas and AI traffic
continues through Mihomo policy. A 2 GiB direct transfer produced bindings on
both PPE engines, averaged about 538 Mbit/s and kept Mihomo below 3 percent.
Short repeated transfers varied substantially, so binding is proven but
a universal throughput gain is not claimed.

The earlier RC2 trial enabled firewall4's generic hardware flowtable while its
software flowtable was also active. That created `flags offload` but did not
produce PPE bindings. Removing the nft flowtable exposed the already-enabled
MediaTek HNAT path and produced real bindings. Use `e87n-offload-status` to
distinguish these states. Keep IPv6 WAN autostart and LAN RA disabled while
Mihomo IPv6 handling is disabled, otherwise IPv6 can bypass the proxy policy.
Detailed live-router evidence and the drop-counter A/B are in `docs/hnat-validation.md`.

The complete `085af11b` `e87n-openclash` hardware-validation image has replaced
the running firmware. Its upgrade retained the private OpenClash YAML and LuCI
package while preserving the user's service state. The repository history,
GitHub secret-scanning alerts, Actions logs/artifacts and published Release
assets were checked before v0.3.0; no known private node credential, controller
secret, root credential or Wi-Fi key was found. Private YAML, controller
credentials and node secrets remain deployment state and must not be copied
into source, build evidence, Actions artifacts or Release assets.
