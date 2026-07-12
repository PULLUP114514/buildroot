################################################################################
#
# epass_applications
#
################################################################################

EPASS_APPLICATIONS_VERSION = 813cf11f09c95442f159940cc8ccde3516892c51
EPASS_APPLICATIONS_SITE = https://github.com/rhodesepass/epass-applications.git
EPASS_APPLICATIONS_SITE_METHOD = git
EPASS_APPLICATIONS_GIT_SUBMODULES = YES
EPASS_APPLICATIONS_DEPENDENCIES = \
	dosfstools \
	e2fsprogs \
	epass-fonts \
	freetype \
	libdrm \
	libpng
EPASS_APPLICATIONS_CONF_OPTS = -DBUILD_SHARED_LIBS=OFF

# Install only the applications component, excluding bundled LVGL development files.
# Relative CMake destinations plus this prefix produce /app/<app_folder>/.
define EPASS_APPLICATIONS_INSTALL_TARGET_CMDS
	DESTDIR=$(TARGET_DIR) $(BR2_CMAKE) \
		--install $(@D) \
		--prefix /app \
		--component applications
endef

$(eval $(cmake-package))
