#!/bin/sh
cp board/rhodesisland/epass/scripts/ubinize-rootfs.cfg "${BINARIES_DIR}/ubinize-rootfs.cfg"
cp board/rhodesisland/epass/scripts/gensdimage.py "${BINARIES_DIR}/gensdimage.py"
cp board/rhodesisland/epass/scripts/kernel.its "${BINARIES_DIR}"

MKIMAGE="${HOST_DIR}/bin/mkimage"
IMAGE_ITS="kernel.its"
OUTPUT_NAME="boot.itb"


cd "${BINARIES_DIR}"
fakeroot sh -c "rm -r rootfs"
fakeroot sh -c "rm ubi.img"
mkdir rootfs

echo ============ start building NAND image ============

echo "building rootfs..."
fakeroot sh -c "tar xf rootfs.tar -C rootfs/"
fakeroot sh -c 'mkfs.ubifs -x lzo -F -m 2048 -e 126976 -c 922 -o rootfs_ubifs.img -r rootfs'
fakeroot sh -c 'ubinize -o rootfs_ubi.img -m 2048 -p 131072 -O 2048 -s 2048 ubinize-rootfs.cfg -v'

echo "building kernel blob..."
cd "${BINARIES_DIR}"
"${MKIMAGE}" -f ${IMAGE_ITS} ${OUTPUT_NAME}

echo ============ start building SD Card image ============
python3 gensdimage.py


rm ubinize-rootfs.cfg
rm -f rootfs_ubifs.img
fakeroot sh -c "rm -r rootfs"

