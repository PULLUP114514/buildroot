# ePass NAND storage layout

0x000000 ~ 0xFA000 U-Boot SPL + U-boot
0xFA000 ~ 0x100000 boot environment
0x100000 ~ 0x200000 DTBs and splash FIT
0x200000 ~ 0x700000 Kernel FIT
0x700000 ~ 0x8000000 Rootfs

these layout is shared between U-Boot and Kernel.

The two FIT images are packed into one 6 MiB boot.itb for DFU compatibility.

if you want to change the partition layout, you need to change the mtdparts in uboot.defconfig and devicetree/linux/base/epass.dtsi and uboot.env.