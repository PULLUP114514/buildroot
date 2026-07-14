#!/bin/sh
cp board/rhodesisland/epass/scripts/ubinize-rootfs.cfg "${BINARIES_DIR}/ubinize-rootfs.cfg"
cp board/rhodesisland/epass/scripts/gensdimage.py "${BINARIES_DIR}/gensdimage.py"
cp board/rhodesisland/epass/scripts/kernel.its "${BINARIES_DIR}"
cp board/rhodesisland/epass/scripts/dtbs.its "${BINARIES_DIR}"
cp board/rhodesisland/epass/logo/*.rgb565.gz "${BINARIES_DIR}"

MKIMAGE="${HOST_DIR}/bin/mkimage"
DTBS_SLOT_SIZE=1048576
KERNEL_SLOT_SIZE=5242880

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

echo "building bad-block-tolerant boot image..."
dd if=/dev/zero bs="${DTBS_SLOT_SIZE}" count=1 2>/dev/null | tr '\000' '\377' > boot.itb || exit 1
dd if=dtbs.itb of=boot.itb conv=notrunc 2>/dev/null || exit 1
dd if=kernel.itb of=boot.itb bs="${DTBS_SLOT_SIZE}" seek=1 conv=notrunc 2>/dev/null || exit 1

boot_size=$(wc -c < boot.itb)
expected_boot_size=$((DTBS_SLOT_SIZE + kernel_size))
if [ "${boot_size}" -ne "${expected_boot_size}" ]; then
    echo "boot.itb has unexpected size: ${boot_size} != ${expected_boot_size}" >&2
    exit 1
fi

echo "building bootfs.vfat..."
rm -f bootfs.vfat
# 8 MiB 太小放不下合法的 FAT32（簇数不足），用 FAT16。
# 且必须用 -s 2（1KB 簇）保证簇数 >= 4085，否则按规范会被判定为 FAT12，
# U-Boot/Linux 解析 FAT16 表时会错乱（"Invalid FAT entry"）
dd if=/dev/zero of=bootfs.vfat bs=1M count=8 status=none
"${HOST_DIR}/sbin/mkfs.vfat" -F 16 -s 2 -n BOOT bootfs.vfat
"${HOST_DIR}/bin/mcopy" -i bootfs.vfat dtbs.itb ::dtbs.itb
"${HOST_DIR}/bin/mcopy" -i bootfs.vfat kernel.itb ::kernel.itb

# 默认 env.txt 是给直接 dd 烧卡的用户准备的(没有 xfel 写 env 的机会),
# 可 dd 后在 PC 上挂载 boot 分区改 device_rev/screen。
# bootargs 由 uboot.env 的 ensurecmdline_sd 提供; interface/ext/extracmd 可选, 空则跳过。
# xfel+DFU 流程里 sdflash 的 fatwrite 会用实际值覆盖它。
cat > env.txt << 'EOF'
device_rev=0.6
screen=hsd
EOF
"${HOST_DIR}/bin/mcopy" -i bootfs.vfat env.txt ::env.txt

cat > README.txt << 'EOF'
This is the boot partition of the ePass SD card image.

IMPORTANT: env.txt defaults to device_rev=0.6, screen=hsd.
If your hardware differs, edit env.txt BEFORE first boot,
otherwise the device may boot with a wrong device tree / screen driver.

  device_rev  board revision: 0.1 / 0.2 / 0.3 / 0.5 / 0.6
  screen      panel type: hsd / boe / laowu / hsd_nv3052
  interface   optional interface overlays (space separated), e.g. i2c0 uart1
  ext         optional extension overlays, e.g. cardkb es8311_sound
  extracmd    optional extra kernel cmdline (appended last)

重要: env.txt 默认 device_rev=0.6, screen=hsd。
如果与你的硬件不符, 请在首次开机前修改 env.txt,
否则设备可能用错误的设备树/屏幕驱动启动。

Do not touch dtbs.itb / kernel.itb.
EOF
"${HOST_DIR}/bin/mcopy" -i bootfs.vfat README.txt ::README.txt

echo ============ start building SD Card image ============
python3 gensdimage.py


rm ubinize-rootfs.cfg
rm -f rootfs_ubifs.img
fakeroot sh -c "rm -r rootfs"

