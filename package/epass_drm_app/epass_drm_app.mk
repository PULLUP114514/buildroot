################################################################################
#
# epass_drm_app
#
################################################################################


EPASS_DRM_APP_VERSION = a2.7.0
EPASS_DRM_APP_SITE = https://github.com/rhodesepass/drm_app_neo.git
EPASS_DRM_APP_SITE_METHOD = git
EPASS_DRM_APP_DEPENDENCIES = libcedarx libcedarc libdrm libpng libevdev jpeg-turbo
EPASS_DRM_APP_GIT_SUBMODULES = YES
EPASS_DRM_APP_CONF_OPTS = -DBUILD_SHARED_LIBS=OFF --fresh

define EPASS_DRM_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/epass_drm_app $(TARGET_DIR)/root/
endef


$(eval $(cmake-package))
