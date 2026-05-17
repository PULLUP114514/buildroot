################################################################################
#
# epass_usb_responder
#
################################################################################


EPASS_USB_RESPONDER_VERSION = e57e74b8bf0611091e9fb6841ffa84be78260f03 
EPASS_USB_RESPONDER_SITE = https://github.com/rhodesepass/epass_usb_responder.git
EPASS_USB_RESPONDER_SITE_METHOD = git
EPASS_USB_RESPONDER_DEPENDENCIES = 
EPASS_USB_RESPONDER_CONF_OPTS = 

define EPASS_USB_RESPONDER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/usb_responder $(TARGET_DIR)/usr/bin/
endef


$(eval $(cmake-package))
