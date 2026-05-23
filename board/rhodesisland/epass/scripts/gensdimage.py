#!/usr/bin/env python3

# genimage is so bad, so we need to write a python script to generate the sdcard image

import os

with open("sd_image.img", "wb") as fout:
    # u-boot
    fout.write(b"\x00" * 0x1000)
    with open("u-boot-sunxi-with-spl.bin","rb") as fin:
        fout.write(fin.read())