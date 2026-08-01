include $(TOPDIR)/rules.mk

PKG_NAME:=fancontrol
PKG_VERSION:=1.26.05.09.02
PKG_RELEASE:=3
PKG_LICENSE:=Proprietary
PKG_MAINTAINER:=EN87 OpenWrt

include $(INCLUDE_DIR)/package.mk

define Package/fancontrol
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=EdgePi E87N fan control daemon (vendor binary)
  DEPENDS:=+libc +libgcc
endef

define Package/fancontrol/description
  Vendor fancontrol binary, init script, and default UCI config from the
  original E87N vendor firmware release 1.26.05.09.02.
endef

define Build/Compile
endef

define Package/fancontrol/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) ./files/usr/bin/fancontrol $(1)/usr/bin/fancontrol
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/fancontrol $(1)/etc/init.d/fancontrol
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/fancontrol $(1)/etc/config/fancontrol
endef

define Package/fancontrol/conffiles
/etc/config/fancontrol
endef

$(eval $(call BuildPackage,fancontrol))
