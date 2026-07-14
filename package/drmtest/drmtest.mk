################################################################################
#
# drmtest
#
################################################################################

DRMTEST_VERSION = 1.0
DRMTEST_SITE = board/rhodesisland/epass/tools
DRMTEST_SITE_METHOD = local
DRMTEST_LICENSE = proprietary
DRMTEST_DEPENDENCIES = libdrm

define DRMTEST_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		-I$(STAGING_DIR)/usr/include/libdrm \
		$(@D)/drmtest.c -ldrm -o $(@D)/drmtest
endef

define DRMTEST_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/drmtest $(TARGET_DIR)/usr/bin/drmtest
endef

$(eval $(generic-package))
