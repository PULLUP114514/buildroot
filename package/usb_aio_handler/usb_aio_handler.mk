################################################################################
#
# usb_aio_handler
#
################################################################################

# 本地源码树，靠 local.mk 的 USB_AIO_HANDLER_OVERRIDE_SRCDIR 提供

USB_AIO_HANDLER_VERSION = 5b0df6ffd2ec859828b57e99d18016a836fb15d8
USB_AIO_HANDLER_SITE = https://github.com/rhodesepass/usb_aio_handler.git
USB_AIO_HANDLER_SITE_METHOD = git
USB_AIO_HANDLER_DEPENDENCIES =


USB_AIO_HANDLER_CONF_OPTS = -DBUILD_SHARED_LIBS=OFF

define USB_AIO_HANDLER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/usb_aio_handler $(TARGET_DIR)/usr/sbin/usb_aio_handler
	$(INSTALL) -D -m 0755 $(@D)/usbaioctl $(TARGET_DIR)/usr/sbin/usbaioctl
endef

$(eval $(cmake-package))
