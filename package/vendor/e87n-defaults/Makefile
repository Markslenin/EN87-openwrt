include $(TOPDIR)/rules.mk

PKG_NAME:=e87n-defaults
PKG_VERSION:=1
PKG_RELEASE:=4
PKG_LICENSE:=GPL-2.0-or-later
PKG_MAINTAINER:=EN87 OpenWrt

include $(INCLUDE_DIR)/package.mk

define Package/e87n-defaults
  SECTION:=base
  CATEGORY:=Base system
  TITLE:=EdgePi E87N safe first-boot defaults
  DEPENDS:=+luci-ssl-openssl +wifi-scripts +wpad-openssl +iw-full +iwinfo +pciutils
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
	$(INSTALL_BIN) ./files/96-e87n-mt7922-wireless $(1)/etc/uci-defaults/96-e87n-mt7922-wireless
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/e87n-mt7922-check $(1)/etc/init.d/e87n-mt7922-check
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/usr/sbin/e87n-offload-status $(1)/usr/sbin/e87n-offload-status
	$(INSTALL_BIN) ./files/usr/sbin/e87n-mt7922-status $(1)/usr/sbin/e87n-mt7922-status
	$(INSTALL_BIN) ./files/usr/sbin/e87n-wifi-profile $(1)/usr/sbin/e87n-wifi-profile
endef

define Package/e87n-defaults/conffiles
/etc/crontabs/root
endef

$(eval $(call BuildPackage,e87n-defaults))
