include $(TOPDIR)/rules.mk

PKG_NAME:=e87n-defaults
PKG_VERSION:=1
PKG_RELEASE:=3
PKG_LICENSE:=GPL-2.0-or-later
PKG_MAINTAINER:=EN87 OpenWrt

include $(INCLUDE_DIR)/package.mk

define Package/e87n-defaults
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=EdgePi E87N safe first-boot defaults
  DEPENDS:=+luci-ssl-openssl
  PKGARCH:=all
endef

define Package/e87n-defaults/description
  Idempotent board defaults that remain safe when sysupgrade preserves an
  existing configuration.
endef

define Build/Compile
endef

define Package/e87n-defaults/install
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/95-e87n-https $(1)/etc/uci-defaults/95-e87n-https
	$(INSTALL_DIR) $(1)/etc/crontabs
	$(INSTALL_CONF) ./files/etc/crontabs/root $(1)/etc/crontabs/root
	$(INSTALL_BIN) ./files/zz-e87n-network-policy $(1)/etc/uci-defaults/zz-e87n-network-policy
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/usr/sbin/e87n-offload-status $(1)/usr/sbin/e87n-offload-status
endef

define Package/e87n-defaults/conffiles
/etc/crontabs/root
endef

$(eval $(call BuildPackage,e87n-defaults))
