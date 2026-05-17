################################################################################
#
# epassctl
#
################################################################################


EPASSCTL_VERSION = 5e283be86e1c59de92ada732575cc2fe0cc43d47
EPASSCTL_SITE = https://github.com/rhodesepass/epassctl.git
EPASSCTL_SITE_METHOD = git
EPASSCTL_DEPENDENCIES = 
EPASSCTL_CONF_OPTS = 

define EPASSCTL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/epassctl $(TARGET_DIR)/usr/bin/
endef

$(eval $(cmake-package))
