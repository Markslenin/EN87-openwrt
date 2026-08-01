#!/usr/bin/env python3
from pathlib import Path
import ipaddress
import yaml

path = Path("configs/e87n-openclash.example.yaml")
data = yaml.safe_load(path.read_text(encoding="utf-8"))

assert "tun" not in data, "OpenClash, not the profile, owns TUN setup"
assert "sniffer" not in data, "sniffing stays disabled unless a measured need appears"
assert data["log-level"] == "warning"
assert data["ipv6"] is False

dns = data["dns"]
assert dns["enable"] is True
assert dns["enhanced-mode"] == "fake-ip"
assert dns["ipv6"] is False
assert dns["respect-rules"] is False
assert dns["prefer-h3"] is False
assert "fallback" not in dns
assert "dns.google" not in path.read_text(encoding="utf-8")
expected_dns = {
    "https://dns.alidns.com/dns-query",
    "https://doh.pub/dns-query",
}
for key in ("nameserver", "proxy-server-nameserver", "direct-nameserver"):
    assert set(dns[key]) == expected_dns

proxies = data["proxies"]
assert len(proxies) == 1
assert proxies[0]["name"] == "US-VPS"
assert ipaddress.ip_address(proxies[0]["server"]).is_private

groups = {group["name"]: group for group in data["proxy-groups"]}
proxy_group = groups["PROXY"]
assert proxy_group["type"] == "fallback"
assert proxy_group["proxies"] == ["US-VPS"]
assert "DIRECT" not in proxy_group["proxies"]
assert proxy_group["lazy"] is True
assert proxy_group["interval"] >= 600

expected_providers = {
    "private_domain": "domain",
    "cn_domain": "domain",
    "google_domain": "domain",
    "ai_domain": "domain",
    "private_ip": "ipcidr",
    "cn_ip": "ipcidr",
}
providers = data["rule-providers"]
assert set(providers) == set(expected_providers)
paths = set()
for name, behavior in expected_providers.items():
    provider = providers[name]
    assert provider["type"] == "http"
    assert provider["behavior"] == behavior
    assert provider["format"] == "mrs"
    assert provider["interval"] == 86400
    assert provider["proxy"] == "US-VPS"
    assert 0 < provider["size-limit"] <= 4194304
    assert provider["path"].startswith("./ruleset/")
    assert provider["path"].endswith(".mrs")
    assert provider["url"].startswith(
        "https://raw.githubusercontent.com/MetaCubeX/meta-rules-dat/"
    )
    paths.add(provider["path"])
assert len(paths) == len(providers)

rules = data["rules"]
assert rules[-1] == "MATCH,PROXY"
assert not any(rule == "MATCH,DIRECT" for rule in rules)
print("E87N OpenClash example checks passed")
