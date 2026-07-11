# 本地源码 override（BR2_PACKAGE_OVERRIDE_FILE 指到这里）。
# 没有这个文件时 usb_aio_handler 会从 github 拉钉死的 initial commit —— 永远是旧代码。
USB_AIO_HANDLER_OVERRIDE_SRCDIR = /f1c_epass/usb_aio_handler
EPASS_DRM_APP_OVERRIDE_SRCDIR = /f1c_epass/drm_app_neo
