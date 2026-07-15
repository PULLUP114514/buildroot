################################################################################
#
# battery_hwcd
#
################################################################################

BATTERY_HWCD_VERSION = 1.0
BATTERY_HWCD_SITE = board/rhodesisland/epass/tools
BATTERY_HWCD_SITE_METHOD = local
BATTERY_HWCD_LICENSE = proprietary

define BATTERY_HWCD_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) \
		$(@D)/battery_hwcd.c -o $(@D)/battery_hwcd
endef

define BATTERY_HWCD_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/battery_hwcd $(TARGET_DIR)/usr/sbin/battery_hwcd
endef

$(eval $(generic-package))
