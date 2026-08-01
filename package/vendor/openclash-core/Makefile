include $(TOPDIR)/rules.mk

PKG_NAME:=openclash-core
PKG_VERSION:=1.19.29
PKG_RELEASE:=1
PKG_SOURCE:=mihomo-linux-arm64-v$(PKG_VERSION).gz
PKG_SOURCE_URL:=https://github.com/MetaCubeX/mihomo/releases/download/v$(PKG_VERSION)/
PKG_HASH:=9a868b5e4e0ad91d9d71e1b41b0cfce78aaba44360c30df74a723f8e3926a86c
PKG_LICENSE:=MIT
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=EN87 OpenWrt

include $(INCLUDE_DIR)/package.mk

define Package/openclash-core
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Web Interfaces
  TITLE:=Pinned Mihomo core for OpenClash on E87N
  URL:=https://github.com/MetaCubeX/mihomo
  DEPENDS:=+luci-app-openclash
endef

define Package/openclash-core/description
  Official statically linked ARM64 Mihomo binary, pinned and hash-verified for
  the optional E87N OpenClash image profile.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	gzip -dc $(DL_DIR)/$(PKG_SOURCE) > $(PKG_BUILD_DIR)/clash_meta
	chmod 0755 $(PKG_BUILD_DIR)/clash_meta
endef

define Build/Compile
endef

define Package/openclash-core/install
	$(INSTALL_DIR) $(1)/etc/openclash/core
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/clash_meta $(1)/etc/openclash/core/clash_meta
	$(INSTALL_DIR) $(1)/usr/share/licenses/openclash-core
	$(INSTALL_DATA) ./LICENSE $(1)/usr/share/licenses/openclash-core/LICENSE
endef

$(eval $(call BuildPackage,openclash-core))
