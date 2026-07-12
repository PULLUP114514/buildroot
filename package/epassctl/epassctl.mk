################################################################################
#
# epassctl
#
################################################################################


EPASSCTL_VERSION = 6da759f72c5b2f33d92377db350e8a9b4074f664
EPASSCTL_SITE = https://github.com/rhodesepass/epassctl.git
EPASSCTL_SITE_METHOD = git
EPASSCTL_DEPENDENCIES = 
EPASSCTL_CONF_OPTS = 

define EPASSCTL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/epassctl $(TARGET_DIR)/usr/bin/
endef

$(eval $(cmake-package))
