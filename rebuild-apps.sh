#!/usr/bin/env bash
# 强制重建自研应用包，并用 host usbaiohost(cp) 推到已连接设备。
set -euo pipefail

cd "$(dirname "$(readlink -f "$0")")"

epcp() {
	output/host/bin/usbaiohost cp "$@"
}

PACKAGES=(
	usb_aio_handler
	epass_drm_app
	epass_applications
	epassctl
)

echo "==> dirclean: ${PACKAGES[*]}"
make "${PACKAGES[@]/%/-dirclean}"

echo "==> make -j16"
make -j16

if [[ ! -x output/host/bin/usbaiohost ]]; then
	echo "错误: 找不到 output/host/bin/usbaiohost，请确认 BR2_PACKAGE_HOST_USB_AIO_HANDLER=y" >&2
	exit 1
fi

TARGET=output/target

echo "==> 安装到设备 (usbaiohost cp)"

# usb_aio_handler -> /usr/sbin/
epcp \
	"$TARGET/usr/sbin/usb_aio_handler" \
	"$TARGET/usr/sbin/usbaioctl" \
	/usr/sbin/

# epassctl -> /usr/bin/
epcp "$TARGET/usr/bin/epassctl" /usr/bin/

# epass_drm_app -> /root/
epcp "$TARGET/root/app_360" "$TARGET/root/app_720" /root/
epcp -r "$TARGET/root/res" /root/

# epass_applications -> /app/<name>/
shopt -s nullglob
app_dirs=("$TARGET"/app/*/)
if ((${#app_dirs[@]} == 0)); then
	echo "错误: $TARGET/app/ 下没有应用目录" >&2
	exit 1
fi
epcp -r "${app_dirs[@]}" /app/

echo "==> 完成"
