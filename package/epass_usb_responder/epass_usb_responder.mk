################################################################################
#
# epass_usb_responder
#
################################################################################


EPASS_USB_RESPONDER_VERSION = 2aa8de07d0753821c27effdf4950c8bad815c889
EPASS_USB_RESPONDER_SITE = https://github.com/rhodesepass/epass_usb_responder.git
EPASS_USB_RESPONDER_SITE_METHOD = git
EPASS_USB_RESPONDER_DEPENDENCIES = 
EPASS_USB_RESPONDER_CONF_OPTS = 

define EPASS_USB_RESPONDER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/usb_responder $(TARGET_DIR)/usr/bin/
endef


$(eval $(cmake-package))
