# Vendor component provenance

The project builds open source integration code around two retained vendor
binaries. Their hashes are recorded so replacements are deliberate and
reviewable.

| Component | Installed path | License | SHA-256 |
| --- | --- | --- | --- |
| Native E87N renderer | `/usr/sbin/display-e87n` | GPL-2.0-or-later | built from `package/vendor/display-control/src/display-e87n.c` |
| Original display fallback | `/usr/sbin/display` | Proprietary | `277f96d5990277145b109845b24d76e1df3ece8dedb28e280fcb08784dff6f37` |
| Fan daemon | `/usr/sbin/fancontrol` | Proprietary | `dede1288565279080e64dff8e13148e3a7db27560825aebdbdc010474031addc` |
| Oswald font | `/usr/share/display-e87n/Oswald.ttf` | OFL-1.1 | `5b3816f8c8ac87989020892769ece5d437a9f21f6d2b1d355bdfb64ce7bcb8e6` |

Do not replace a proprietary binary without recording its source, license, and
new digest here. The native renderer and service scripts are the preferred path;
the original display program exists only as a compatibility fallback.
