import subprocess
import struct
import time


def dfu_device_present():
    try:
        result = subprocess.run(["dfu-util", "-l"], capture_output=True, text=True)
        return "Found DFU: [1f3a:1010]" in result.stdout
    except Exception:
        return False


def wait_for_dfu():
    print("等待设备重启进入DFU模式...", end="", flush=True)
    while True:
        print(".", end="", flush=True)
        if dfu_device_present():
            print("\n抓到了!")
            break
        time.sleep(0.5)


def make_bootinfo(env_payload, boot_type):
    with open(".bootinfo.txt", "wb") as f:
        f.write(b"Mostima_")
        f.write(bytes([boot_type]))
        f.write(b"\x00\x00\x00")
        f.write(struct.pack("<I", len(env_payload)))
        f.write(env_payload)


def xfel_boot():
    subprocess.run(["xfel", "ddr"])
    subprocess.run(["xfel", "write", "0x80000000", ".bootinfo.txt"])
    subprocess.run(["xfel", "write", "0x81700000", "output/images/u-boot.bin"])
    subprocess.run(["xfel", "exec", "0x81700000"])


def flash_nand(rev, screen, files):
    env_payload = (
        "bootargs=console=ttyS0,115200 panic=5 rootwait "
        "mtdparts=spi0.0:896K(u-boot)ro,128K(bootenv),6M(boot),-(rootfs) "
        "root=ubi0:rootfs rw rootfstype=ubifs ubi.mtd=3 "
        "ubi.fm_autoconvert=1\n"
        f"device_rev={rev}\n"
        f"screen={screen}\n"
        "interface=\n"
        "ext=\n"
    ).encode("utf-8") + b"\x00"

    make_bootinfo(env_payload, 0x01)
    xfel_boot()

    wait_for_dfu()
    subprocess.run(
        ["dfu-util", "-d", "1f3a:1010", "-a", "uboot", "-D", files["uboot"]]
    )
    print("烧录uboot分区完成，等待2秒后开始烧录boot分区...")
    time.sleep(2)
    wait_for_dfu()
    subprocess.run(
        ["dfu-util", "-d", "1f3a:1010", "-a", "boot", "-D", files["boot"]]
    )
    print("烧录boot分区完成，等待2秒后开始烧录rootfs分区...")
    time.sleep(2)
    wait_for_dfu()
    subprocess.run(
        [
            "dfu-util",
            "-d",
            "1f3a:1010",
            "-R",
            "-a",
            "rootfs",
            "-D",
            files["rootfs"],
        ]
    )


def flash_sd(rev, screen, files):
    env_payload = (
        "bootargs=console=ttyS0,115200 panic=5 rootwait "
        "root=/dev/mmcblk0p2 rw rootfstype=ext4\n"
        f"device_rev={rev}\n"
        f"screen={screen}\n"
        "interface=\n"
        "ext=\n"
    ).encode("utf-8") + b"\x00"

    make_bootinfo(env_payload, 0x02)
    xfel_boot()

    wait_for_dfu()
    subprocess.run(
        ["dfu-util", "-d", "1f3a:1010", "-a", "uboot", "-D", files["uboot"]]
    )
    print("烧录 SD u-boot 完成，等待2秒后开始烧录 boot 分区...")
    time.sleep(2)
    wait_for_dfu()
    subprocess.run(
        ["dfu-util", "-d", "1f3a:1010", "-a", "boot", "-D", files["boot"]]
    )
    print("烧录 SD boot 分区完成，等待2秒后开始烧录 rootfs 分区...")
    time.sleep(2)
    wait_for_dfu()
    subprocess.run(
        [
            "dfu-util",
            "-d",
            "1f3a:1010",
            "-R",
            "-a",
            "rootfs",
            "-D",
            files["rootfs"],
        ]
    )


if __name__ == "__main__":
    # flash_nand(
    #     "0.3",
    #     "hsd",
    #     {
    #         "uboot": "output/images/u-boot-sunxi-with-nand-spl.bin",
    #         "boot": "output/images/boot.itb",
    #         "rootfs": "output/images/rootfs_ubi.img",
    #     },
    # )

    flash_sd(
        "0.3",
        "hsd",
        {
            "uboot": "output/images/u-boot-sunxi-with-spl.bin",
            "boot": "output/images/bootfs.vfat",
            "rootfs": "output/images/rootfs.ext4",
        },
    )
