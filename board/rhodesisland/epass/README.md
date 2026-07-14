# ePass NAND storage layout

```
0x000000 ~ 0xE0000 U-Boot SPL + U-boot
0xE0000 ~ 0x100000 boot environment (1 eraseblock, 0x20000-aligned)
0x100000 DTBs and splash FIT (1 MiB logical slot)
0x200000 Kernel FIT nominal start
0x700000 ~ 0x8000000 Rootfs
```

these layout is shared between U-Boot and Kernel.

The two FIT images are packed into one boot.itb for DFU compatibility. The
image ends after kernel.itb instead of being padded to 6 MiB, leaving the rest
of the boot partition as bad-block reserve. If DFU skips bad blocks before the
kernel FIT, its physical start moves by whole eraseblocks; U-Boot scans from
the nominal offset to find it.

Partitions come from fixed-partitions in both
devicetree/linux/base/epass.dtsi (kernel) and
devicetree/uboot/suniv-f1c100s-generic.dts (U-Boot / DFU). Keep those two in
sync when changing the layout.

# ePass SD Card

* raw offset 8 KiB: u-boot-sunxi-with-spl.bin (same place DFU writes with
  `uboot raw 0x10 0x7f0`).
* mmcblkxp1: 8 MiB FAT16 boot partition, 1 MiB offset.
  * dtbs.itb (device trees and splash)
  * kernel.itb (kernel)
  * env.txt
* mmcblkxp2: 128 MiB ext4 root filesystem.
  * The flashed image is only 20 MiB (BR2_TARGET_ROOTFS_EXT2_SIZE) to keep
    full-speed USB DFU transfers short; S00sdsetup grows it to the full
    partition with resize2fs on first boot.
* mmcblkxp3: FAT "share" partition covering the rest of the card.
  * Formatted by S00sdsetup on first boot, and mounted at /sd (same path as
    the external SD card when booting from NAND).

There are two ways to flash an SD card:

* **xfel + DFU** (`flash.py` → `flash_sd`): U-Boot's `sdflash` writes the MBR
  sized to the actual card (share is `size=-`), DFU downloads u-boot / boot /
  rootfs, and `fatwrite` stores an env.txt with the device_rev/screen passed
  to flash.py.
* **dd the whole-card image** (`output/images/sd_image.img`, built by
  `gensdimage.py`):

  ```
  dd if=sd_image.img of=/dev/sdX bs=1M   # the whole card, not a partition
  ```

  The image is ~138 MiB: MBR + u-boot + boot partition + rootfs + 1 MiB of
  zeros at the start of share (wipes stale filesystem signatures so
  S00sdsetup reformats it). Since the card size is unknown at build time, the
  MBR entry for share claims to extend to the 2 TB MBR addressing limit; the
  kernel truncates it to the real end of the card at boot, and S00sdsetup
  rewrites the on-disk size field to the real value on first boot so the
  partition table also looks sane in card readers.

  The image carries a default env.txt (device_rev=0.6, screen=hsd); mount the
  boot partition on a PC and edit it for other hardware (see README.txt on
  the boot partition).