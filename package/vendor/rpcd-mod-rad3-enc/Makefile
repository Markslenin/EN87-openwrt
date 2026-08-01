# Backported from ImmortalWrt LuCI openwrt-25.12 to satisfy the matching
# luci-app-radicale3 dependency that is already present in the 24.10 feed.

include $(TOPDIR)/rules.mk

PKG_NAME:=rpcd-mod-rad3-enc
PKG_VERSION:=20260109
PKG_RELEASE:=1
PKG_LICENSE:=Apache-2.0

include $(INCLUDE_DIR)/package.mk

define Package/rpcd-mod-rad3-enc
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Radicale v3 hashing RPC module
  DEPENDS:=+rpcd +python3 +python3-passlib +radicale3
  PROVIDES:=rpcd-mod-rad2-enc
  PKGARCH:=all
endef

define Package/rpcd-mod-rad3-enc/description
  Python 3 password hashing RPC module used by the Radicale v3 LuCI app.
endef

define Build/Compile
endef

define Package/rpcd-mod-rad3-enc/install
	$(INSTALL_DIR) $(1)/usr/libexec/rpcd
	$(INSTALL_BIN) ./files/rad3-enc $(1)/usr/libexec/rpcd/rad3-enc
endef

define Package/rpcd-mod-rad3-enc/postinst
#!/bin/sh
[ -n "$$IPKG_INSTROOT" ] || /etc/init.d/rpcd reload
endef

$(eval $(call BuildPackage,rpcd-mod-rad3-enc))
