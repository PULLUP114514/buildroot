################################################################################
#
# epass_drm_app
#
################################################################################


EPASS_DRM_APP_VERSION = 8464a6e76b220b0eecc299074d35091a3f3c7f8e
EPASS_DRM_APP_SITE = https://github.com/rhodesepass/drm_app_neo.git
EPASS_DRM_APP_SITE_METHOD = git
# epass-fonts: 提供 pkg-config 'epass-fonts', app 构建期据此取共享字体目录,
# 不再自带字体 (字体由 epass-fonts 包装到 /usr/share/fonts/epass)。
EPASS_DRM_APP_DEPENDENCIES = freetype libdrm libpng libevdev epass-fonts
EPASS_DRM_APP_GIT_SUBMODULES = YES
EPASS_DRM_APP_CONF_OPTS = -DBUILD_SHARED_LIBS=OFF --fresh

define EPASS_DRM_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/app_360 $(TARGET_DIR)/root/app_360
	$(INSTALL) -D -m 0755 $(@D)/app_720 $(TARGET_DIR)/root/app_720
	cp -a $(@D)/res $(TARGET_DIR)/root/
endef


$(eval $(cmake-package))
