include $(TOPDIR)/rules.mk

PKG_NAME:=display-control
PKG_VERSION:=1.26.05.09.02
PKG_RELEASE:=15
PKG_LICENSE:=Proprietary OFL-1.1
PKG_LICENSE_FILES:=files/usr/share/display-e87n/OFL.txt
PKG_MAINTAINER:=HiGoROS

include $(INCLUDE_DIR)/package.mk

define Package/display-control
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=EdgePi E87N configurable framebuffer display service
  DEPENDS:=+libc +libgcc +libfreetype +libuci
endef

define Package/display-control/description
  Configurable native framebuffer dashboard for the EdgePi E87N, plus
  the original vendor display binary as a compatibility fallback.
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-Wall -Wextra -Wformat=2 \
		-I$(STAGING_DIR)/usr/include/freetype2 \
		-o $(PKG_BUILD_DIR)/display-e87n ./src/display-e87n.c \
		$(TARGET_LDFLAGS) -lfreetype -luci -lm
endef

define Package/display-control/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./files/usr/sbin/display $(1)/usr/sbin/display
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/display-e87n $(1)/usr/sbin/display-e87n
	$(INSTALL_BIN) ./files/usr/sbin/display-control $(1)/usr/sbin/display-control
	$(INSTALL_DIR) $(1)/usr/share/display-e87n
	$(INSTALL_DATA) ./files/usr/share/display-e87n/Oswald.ttf $(1)/usr/share/display-e87n/Oswald.ttf
	$(INSTALL_DATA) ./files/usr/share/display-e87n/OFL.txt $(1)/usr/share/display-e87n/OFL.txt
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/display-control $(1)/etc/init.d/display-control
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/display $(1)/etc/config/display
	$(INSTALL_DIR) $(1)/etc/uci-defaults
	$(INSTALL_BIN) ./files/etc/uci-defaults/96-display-e87n $(1)/etc/uci-defaults/96-display-e87n
endef

define Package/display-control/conffiles
/etc/config/display
endef

$(eval $(call BuildPackage,display-control))
