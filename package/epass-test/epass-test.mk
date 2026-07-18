################################################################################
#
# epass-test
#
################################################################################

EPASS_TEST_VERSION = 1.0
EPASS_TEST_SITE = board/rhodesisland/epass/tools
EPASS_TEST_SITE_METHOD = local
EPASS_TEST_LICENSE = proprietary
EPASS_TEST_DEPENDENCIES = libdrm

EPASS_TEST_TOOLS = drmtest c8test

define EPASS_TEST_BUILD_CMDS
	$(foreach tool,$(EPASS_TEST_TOOLS), \
		$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
			-I$(STAGING_DIR)/usr/include/libdrm \
			$(@D)/$(tool).c -ldrm -o $(@D)/$(tool)$(sep))
endef

define EPASS_TEST_INSTALL_TARGET_CMDS
	$(foreach tool,$(EPASS_TEST_TOOLS), \
		$(INSTALL) -D -m 0755 $(@D)/$(tool) \
			$(TARGET_DIR)/usr/bin/$(tool)$(sep))
endef

$(eval $(generic-package))
