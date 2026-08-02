# E87N HNAT validation

This records the live-router acceptance evidence for the RC3 candidate. It is
not a generic MT7987 benchmark and does not claim that Mihomo TUN traffic is
hardware accelerated.

The tested runtime map was `eth1` for WAN and `br-lan` for LAN, with `eth0` and
the MediaTek `hnat` interface in the bridge. Revalidate this map before applying
the result to a different network configuration.

## Validated policy

- OpenClash China IPv4 bypass enabled.
- Firewall4 software and hardware nft flowtables disabled.
- TurboACC engine set to `mediatek_hnat`.
- Ethernet HNAT enabled and IPv6 HNAT disabled.
- WAN6 autostart and LAN IPv6 router advertisements disabled.
- Mihomo continued to route Google and AI rules through `PROXY`.
- IPv4 direct and proxied requests succeeded; an explicit IPv6 request failed
  with no IPv6 default route present.

## 2 GiB direct-flow test

Test object: the first 2 GiB of a USTC Debian DVD image, discarded locally.
The transfer used one IPv4 HTTPS connection from a LAN client.

```text
bytes                 2147483648
elapsed               31.927120 s
average               67262059 B/s (about 538 Mbit/s)
maximum PPE bindings  4 total across PPE0 and PPE1
bound samples         26 of 50 one-second samples
maximum Mihomo CPU    3 percent
WAN RX errors         0
RX overflow           0
RX FCS errors         unchanged at 1 historical event
```

The WAN and LAN software byte counters were lower than the transferred byte
count while PPE entries were bound. This is expected when part of a flow no
longer traverses the normal software accounting path.

## `eth1.rx_dropped` review

The standard `rx_dropped` counter increases at a steady five counts per second.
An A/B test found the same rate in all observed states:

```text
HNAT enabled, idle 30 s:   +150 drops
HNAT disabled, idle 30 s:  +150 drops
HNAT disabled, 512 MiB:    +130 drops over 25 s
HNAT enabled, 2 GiB:       +285 drops over 50 s
```

The counter did not scale materially with throughput or HNAT state. During the
tests `rx_errors`, `rx_overflow`, short/long frame errors and XDP drops stayed
at zero, and the historical FCS count did not increase. The evidence therefore
does not identify HNAT as the cause and does not show physical packet loss.
Treat the steady counter as a driver/core accounting observation and continue
monitoring it; reopen the issue if its rate changes, physical error counters
increase, or application loss becomes measurable.

An additional v0.3.1 investigation identified the source of the steady count.
With no packet socket listener, `rx_dropped` increased by 75 over 15 seconds.
A header-only capture showed the upstream gateway transmitting five broadcast
frames per second with unknown EtherType `0x8300`. While the packet socket was
listening those frames had a protocol consumer and the drop-accounting rate
collapsed; hardware `rx_overflow`, short/long/checksum errors and XDP drops
remained zero, with only one unchanged historical FCS error. This confirms the
counter is Linux unhandled-L2-frame accounting for the gateway broadcasts, not
HNAT congestion or measured application packet loss. No firewall, DTS or
driver workaround is applied.

Use `e87n-offload-status --sample 15` to record the current counter delta and
hardware error boundary without running a traffic benchmark.

## Acceptance boundary

The test proves that direct IPv4 flows can bind to PPE without breaking the
current OpenClash policy. It does not prove that every destination is faster,
that proxy/TUN traffic is offloaded, or that this result transfers to a
different WAN/LAN map. Recheck policy, PPE binding and errors after future DTS,
kernel, firewall or OpenClash changes.
