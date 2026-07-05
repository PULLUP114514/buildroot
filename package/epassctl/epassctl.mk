################################################################################
#
# epassctl
#
################################################################################


EPASSCTL_VERSION = 07b96b87d8b24b4c88ae69c9c1c058bafbde2631
EPASSCTL_SITE = https://github.com/rhodesepass/epassctl.git
EPASSCTL_SITE_METHOD = git
EPASSCTL_DEPENDENCIES = 
EPASSCTL_CONF_OPTS = 

define EPASSCTL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/epassctl $(TARGET_DIR)/usr/bin/
endef

$(eval $(cmake-package))
