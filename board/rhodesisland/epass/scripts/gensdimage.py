#!/usr/bin/env python3
"""生成可直接 dd 整卡烧录的 SD 镜像 sd_image.img。

分区布局与 uboot.env 里 sdflash 写的 MBR 保持一致:
    0x2000            u-boot-sunxi-with-spl.bin (同 DFU "uboot raw 0x10 0x7f0")
    1MiB ~ 9MiB       p1 boot   FAT16, dtbs.itb + kernel.itb + env.txt
    9MiB ~ 137MiB     p2 rootfs ext4(实际只有 20MiB, 首次开机 resize2fs 扩满)
    137MiB ~ 卡末尾   p3 share  FAT, 由首次开机的 S00sdsetup 格式化

share 分区的大小在构建时不知道(取决于用户的卡), 所以 MBR 里先写成
"到 2TB 寻址上限"。内核解析分区表时会把超出卡尾的分区截断到实际末尾
(block/partition-generic.c), S00sdsetup 首次开机再把盘上 MBR 的 size
字段改成实际值, 使读卡器上看分区表也是自洽的。

镜像尾部多带 1MiB 零填充(p3 开头), 抹掉卡上残留的文件系统签名,
避免 S00sdsetup 误以为 share 分区已格式化。
"""

import struct

SECTOR = 512
UBOOT_OFFSET = 0x2000
BOOT_START = 1 * 1024 * 1024
BOOT_SIZE = 8 * 1024 * 1024
ROOTFS_START = BOOT_START + BOOT_SIZE
ROOTFS_SIZE = 128 * 1024 * 1024
SHARE_START = ROOTFS_START + ROOTFS_SIZE
SHARE_WIPE = 1 * 1024 * 1024
MAX_LBA = 0xFFFFFFFF  # MBR 32 位寻址上限(2TB), 也是 SDXC 容量上限


def build_mbr():
    buf = bytearray(SECTOR)
    entries = [
        # (起始扇区, 扇区数, 分区类型, bootable)
        (BOOT_START // SECTOR, BOOT_SIZE // SECTOR, 0x0E, True),
        (ROOTFS_START // SECTOR, ROOTFS_SIZE // SECTOR, 0x83, False),
        (SHARE_START // SECTOR, MAX_LBA - SHARE_START // SECTOR, 0x0C, False),
    ]
    for i, (start, size, sysid, bootable) in enumerate(entries):
        off = 446 + i * 16
        buf[off:off + 16] = struct.pack(
            "<B3sB3sII",
            0x80 if bootable else 0x00,
            b"\xfe\xff\xff",  # CHS 已无意义, 填 LBA 惯用极值
            sysid,
            b"\xfe\xff\xff",
            start,
            size,
        )
    buf[510:512] = b"\x55\xaa"
    return bytes(buf)


def write_at(fout, offset, path, limit, name):
    with open(path, "rb") as fin:
        data = fin.read()
    if len(data) > limit:
        raise SystemExit(f"{name} too large: {len(data)} > {limit}")
    fout.seek(offset)
    fout.write(data)


with open("sd_image.img", "wb") as fout:
    fout.truncate(SHARE_START + SHARE_WIPE)
    fout.write(build_mbr())
    write_at(fout, UBOOT_OFFSET, "u-boot-sunxi-with-spl.bin",
             BOOT_START - UBOOT_OFFSET, "u-boot")
    write_at(fout, BOOT_START, "bootfs.vfat", BOOT_SIZE, "bootfs.vfat")
    write_at(fout, ROOTFS_START, "rootfs.ext4", ROOTFS_SIZE, "rootfs.ext4")

print(f"sd_image.img: {(SHARE_START + SHARE_WIPE) // (1024 * 1024)} MiB, "
      "dd it to a whole SD card (not a partition)")
