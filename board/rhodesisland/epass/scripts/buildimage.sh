#!/bin/sh
cp board/rhodesisland/epass/scripts/ubinize-rootfs.cfg "${BINARIES_DIR}/ubinize-rootfs.cfg"
cp board/rhodesisland/epass/scripts/gensdimage.py "${BINARIES_DIR}/gensdimage.py"
cp board/rhodesisland/epass/scripts/kernel.its "${BINARIES_DIR}"
cp board/rhodesisland/epass/scripts/dtbs.its "${BINARIES_DIR}"
cp board/rhodesisland/epass/logo/*.rgb565.gz "${BINARIES_DIR}"

MKIMAGE="${HOST_DIR}/bin/mkimage"
DTBS_SLOT_SIZE=1048576
KERNEL_SLOT_SIZE=5242880
BOOT_IMAGE_SIZE=6291456

cd "${BINARIES_DIR}"
fakeroot sh -c "rm -r rootfs"
fakeroot sh -c "rm ubi.img"
mkdir rootfs

echo ============ start building NAND image ============

echo "building rootfs..."
fakeroot sh -c "tar xf rootfs.tar -C rootfs/"
fakeroot sh -c 'mkfs.ubifs -x lzo -F -m 2048 -e 126976 -c 922 -o rootfs_ubifs.img -r rootfs'
fakeroot sh -c 'ubinize -o rootfs_ubi.img -m 2048 -p 131072 -O 2048 -s 2048 ubinize-rootfs.cfg -v'

echo "building DTBs and kernel FIT images..."
cd "${BINARIES_DIR}"
rm -f dtbs.itb kernel.itb boot.itb
"${MKIMAGE}" -f dtbs.its dtbs.itb || exit 1
"${MKIMAGE}" -f kernel.its kernel.itb || exit 1

dtbs_size=$(wc -c < dtbs.itb)
kernel_size=$(wc -c < kernel.itb)
if [ "${dtbs_size}" -gt "${DTBS_SLOT_SIZE}" ]; then
    echo "dtbs.itb is too large: ${dtbs_size} > ${DTBS_SLOT_SIZE}" >&2
    exit 1
fi
if [ "${kernel_size}" -gt "${KERNEL_SLOT_SIZE}" ]; then
    echo "kernel.itb is too large: ${kernel_size} > ${KERNEL_SLOT_SIZE}" >&2
    exit 1
fi

echo "building 6 MiB boot partition image..."
dd if=/dev/zero bs="${BOOT_IMAGE_SIZE}" count=1 2>/dev/null | tr '\000' '\377' > boot.itb || exit 1
dd if=dtbs.itb of=boot.itb conv=notrunc 2>/dev/null || exit 1
dd if=kernel.itb of=boot.itb bs="${DTBS_SLOT_SIZE}" seek=1 conv=notrunc 2>/dev/null || exit 1

boot_size=$(wc -c < boot.itb)
if [ "${boot_size}" -ne "${BOOT_IMAGE_SIZE}" ]; then
    echo "boot.itb has unexpected size: ${boot_size} != ${BOOT_IMAGE_SIZE}" >&2
    exit 1
fi

echo ============ start building SD Card image ============
python3 gensdimage.py


rm ubinize-rootfs.cfg
rm -f rootfs_ubifs.img
fakeroot sh -c "rm -r rootfs"

