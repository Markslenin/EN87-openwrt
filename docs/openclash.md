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
