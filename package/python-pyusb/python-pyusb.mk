################################################################################
#
# python-pyusb
#
################################################################################

PYTHON_PYUSB_VERSION = 1.0.0
PYTHON_PYUSB_SOURCE = PyUSB-$(PYTHON_PYUSB_VERSION).tar.gz
PYTHON_PYUSB_SITE = https://pypi.python.org/packages/8a/19/66fb48a4905e472f5dfeda3a1bafac369fbf6d6fc5cf55b780864962652d
PYTHON_PYUSB_LICENSE = BSD-3-Clause
PYTHON_PYUSB_LICENSE_FILES = LICENSE
PYTHON_PYUSB_SETUP_TYPE = distutils
PYTHON_PYUSB_DEPENDENCIES = libusb

# host 侧给 usbaiohost(pyhost 上位机)用。纯 python,构建期不需要 libusb;
# 运行期通过 ctypes 找系统的 libusb-1.0。
HOST_PYTHON_PYUSB_NEEDS_HOST_PYTHON = python3
HOST_PYTHON_PYUSB_DEPENDENCIES =

$(eval $(python-package))
$(eval $(host-python-package))
