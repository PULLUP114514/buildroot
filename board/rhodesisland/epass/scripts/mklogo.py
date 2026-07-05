#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "pillow",
#     "numpy",
# ]
# ///
"""从任意图片生成 boot splash logo(RGB565 LE raw + gzip,供 kernel.its 打包)。

用法:
    uv run mklogo.py logo.png                  # 生成两种屏幕的 logo 到 ../logo/
    uv run mklogo.py logo.png -p mostima       # 只生成 mostima(360x640 屏)
    uv run mklogo.py logo.png -o /tmp/out      # 指定输出目录
    uv run mklogo.py logo.png --preview        # 顺便输出 PNG 预览

图片按等比缩放放进可视区,黑边 letterbox。改完记得重新跑 buildroot 的
make(post-image 会把 logo 打进 boot.itb),然后 DFU 刷 boot 分区。
"""

import argparse
import gzip
import sys
from pathlib import Path

import numpy as np
from PIL import Image

# scanout = DEBE/TCON 出的分辨率;visible = 面板实际可见区(在 scanout 里的偏移)
# mostima 屏 hactive=384 里只有左侧 360 可见(时序对齐用的 24px 在右侧)
PANELS = {
    "mostima": {"scanout": (384, 640), "visible": (360, 640), "offset": (0, 0)},
    "hsd_nv3052": {"scanout": (720, 1280), "visible": (720, 1280), "offset": (0, 0)},
}


def fit_into(img: Image.Image, w: int, h: int) -> Image.Image:
    scale = min(w / img.width, h / img.height)
    nw, nh = max(1, round(img.width * scale)), max(1, round(img.height * scale))
    resized = img.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGB", (w, h), (0, 0, 0))
    canvas.paste(resized, ((w - nw) // 2, (h - nh) // 2))
    return canvas


def to_rgb565le(img: Image.Image) -> bytes:
    a = np.asarray(img.convert("RGB"), dtype=np.uint16)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return px.astype("<u2").tobytes()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("image", type=Path)
    ap.add_argument("-p", "--panel", choices=[*PANELS, "all"], default="all")
    ap.add_argument("-o", "--outdir", type=Path,
                    default=Path(__file__).resolve().parent.parent / "logo")
    ap.add_argument("--preview", action="store_true",
                    help="同时输出 logo-<panel>.preview.png")
    args = ap.parse_args()

    src = Image.open(args.image)
    args.outdir.mkdir(parents=True, exist_ok=True)

    panels = PANELS if args.panel == "all" else {args.panel: PANELS[args.panel]}
    for name, geo in panels.items():
        sw, sh = geo["scanout"]
        vw, vh = geo["visible"]
        ox, oy = geo["offset"]

        frame = Image.new("RGB", (sw, sh), (0, 0, 0))
        frame.paste(fit_into(src, vw, vh), (ox, oy))

        raw = to_rgb565le(frame)
        assert len(raw) == sw * sh * 2

        out = args.outdir / f"logo-{name}.rgb565.gz"
        out.write_bytes(gzip.compress(raw, 9))
        print(f"{out}  scanout {sw}x{sh}, visible {vw}x{vh}, "
              f"{out.stat().st_size} bytes gz")

        if args.preview:
            frame.save(args.outdir / f"logo-{name}.preview.png")

    return 0


if __name__ == "__main__":
    sys.exit(main())
