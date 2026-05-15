#!/bin/sh
cp board/rhodesisland/epass/scripts/ubinize-boot.cfg "${BINARIES_DIR}/ubinize-boot.cfg"
cp board/rhodesisland/epass/scripts/ubinize-rootfs.cfg "${BINARIES_DIR}/ubinize-rootfs.cfg"
cp board/rhodesisland/epass/uEnv.txt "${BINARIES_DIR}/uEnv.txt"
cd "${BINARIES_DIR}"
fakeroot sh -c "rm -r rootfs"
fakeroot sh -c "rm ubi.img"
mkdir rootfs

echo "building rootfs..."
fakeroot sh -c "tar xf rootfs.tar -C rootfs/"
fakeroot sh -c 'mkfs.ubifs -x lzo -F -m 2048 -e 126976 -c 874 -o rootfs_ubifs.img -r rootfs'
fakeroot sh -c 'ubinize -o rootfs_ubi.img -m 2048 -p 131072 -O 2048 -s 2048 ubinize-rootfs.cfg -v'

echo "building boot..."
fakeroot sh -c "mkdir -p boot/dt/"
fakeroot sh -c "cp -r dt/* boot/dt/"
fakeroot sh -c "cp zImage boot/zImage"
fakeroot sh -c "mv uEnv.txt boot/uEnv.txt"
fakeroot sh -c 'mkfs.ubifs -x none -F -m 2048 -e 126976 -c 72 -o boot_ubifs.img -r boot'
fakeroot sh -c 'ubinize -o boot_ubi.img -m 2048 -p 131072 -O 2048 -s 2048 ubinize-boot.cfg -v'

rm ubinize-boot.cfg
rm ubinize-rootfs.cfg
rm -f rootfs_ubifs.img
rm -f boot_ubifs.img
fakeroot sh -c "rm -r rootfs"
fakeroot sh -c "rm -r boot"

