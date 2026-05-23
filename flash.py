import subprocess
import os
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

def flash(rev, screen, files):
    with open(".bootenv.txt", "wb") as f:
        f.write(f"bootargs=console=ttyS0,115200 console=tty0 panic=5 rootwait root=ubi0:rootfs rw rootfstype=ubifs ubi.mtd=2 ubi.fm_autoconvert=1\n".encode("utf-8"))
        f.write(f"device_rev={rev}\n".encode("utf-8"))
        f.write(f"screen={screen}\n".encode("utf-8"))
        f.write(b"\x00")
    subprocess.run(["xfel", "spinand", "erase", "0x100000", "0xC00000"])
    subprocess.run(["xfel", "spinand", "write", "0", files["uboot"]])
    subprocess.run(["xfel", "spinand", "write", "0xfa000", ".bootenv.txt"])
    subprocess.run(["xfel", "reset"])
    wait_for_dfu()
    subprocess.run(["dfu-util", "-d" ,"1f3a:1010", "-a", "boot", "-D", files["boot"]])
    print("烧录boot分区完成，等待2秒后开始烧录rootfs分区...")
    time.sleep(2)
    wait_for_dfu()
    subprocess.run(["dfu-util", "-d" ,"1f3a:1010", "-R", "-a", "rootfs", "-D", files["rootfs"]])

def flash2(rev, screen, files):
    with open(".bootenv.txt", "wb") as f:
        f.write(f"bootargs=console=ttyS0,115200 console=tty0 panic=5 rootwait root=ubi0:rootfs rw rootfstype=ubifs ubi.mtd=2 ubi.fm_autoconvert=1 \n".encode("utf-8"))
        f.write(f"device_rev={rev}\n".encode("utf-8"))
        f.write(f"screen={screen}\n".encode("utf-8"))
        f.write(b"\x00")
    subprocess.run(["xfel", "spinand", "write", "0xfa000", ".bootenv.txt"])
    subprocess.run(["xfel", "reset"])

if __name__ == "__main__":
    flash("0.3", "hsd", {
        "uboot": "output/images/u-boot-sunxi-with-nand-spl.bin",
        "boot": "output/images/boot.itb",
        "rootfs": "output/images/rootfs_ubi.img"
    })
