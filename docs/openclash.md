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

## First configuration

1. Flash the OpenClash-profile image or install both generated packages.
2. Import a local Clash/Mihomo configuration or add a subscription in LuCI.
3. Set a dashboard secret before exposing the controller to LAN clients.
4. Keep the initial mode at `fake-ip`, enable OpenClash, and verify DNS, the
   controller API and an explicit proxied request.
5. Benchmark direct and proxied throughput separately before changing HNAT,
   firewall offload or interface mapping.

Do not assume that HNAT/PPE accelerates traffic processed through Mihomo TUN or
TProxy. Back up `/etc/config/openclash` and `/etc/openclash/` before mode,
firewall or DNS changes.
