################################################################################
#
# python-fonttools
#
# 仅提供 host 变体: epass-fonts 包在 host 侧子集化字体时需要 fonttools。
# 用 github tag 源 (无需 pypi 哈希路径), setuptools 装到 buildroot host python。
#
################################################################################

PYTHON_FONTTOOLS_VERSION = 4.38.0
PYTHON_FONTTOOLS_SITE = $(call github,fonttools,fonttools,$(PYTHON_FONTTOOLS_VERSION))
PYTHON_FONTTOOLS_SETUP_TYPE = setuptools
PYTHON_FONTTOOLS_LICENSE = MIT
PYTHON_FONTTOOLS_LICENSE_FILES = LICENSE

# fonttools 4.x 是 py3-only。此 buildroot 里 host 包默认解释器是 python2, 必须显式
# 指定 python3 (否则 setup.py 在 py2 下 byte-compile 报 SyntaxError)。
HOST_PYTHON_FONTTOOLS_NEEDS_HOST_PYTHON = python3

$(eval $(host-python-package))
