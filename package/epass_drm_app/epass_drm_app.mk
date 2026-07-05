################################################################################
#
# epass_drm_app
#
################################################################################


EPASS_DRM_APP_VERSION = f45e4fdc3f939dcd2e24fee8b6e775414b740f21
EPASS_DRM_APP_SITE = https://github.com/rhodesepass/drm_app_neo.git
EPASS_DRM_APP_SITE_METHOD = git
EPASS_DRM_APP_DEPENDENCIES = freetype libdrm libpng libevdev
EPASS_DRM_APP_GIT_SUBMODULES = YES
EPASS_DRM_APP_CONF_OPTS = -DBUILD_SHARED_LIBS=OFF --fresh

define EPASS_DRM_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/app_360 $(TARGET_DIR)/root/app_360
	$(INSTALL) -D -m 0755 $(@D)/app_720 $(TARGET_DIR)/root/app_720
	cp -a $(@D)/res $(TARGET_DIR)/root/
endef


$(eval $(cmake-package))
