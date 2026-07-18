# DEBE 调色板(C8/index)图层使用说明

对应补丁:`0029-sun4i-backend-debe-layer-palette-mode.patch`

DEBE 的图层支持索引模式:framebuffer 每字节是一个索引,扫描时硬件查
片上 SRAM 里的 256 项 ARGB8888 调色板出色,不占 DRAM 带宽。8bpp 比
RGB565 省一半、比 XRGB8888 省 3/4 的图层读带宽,且**每个调色板项自带
alpha**,可以做带透明的 overlay。

补丁做了两件事:

1. DRM 侧暴露 `DRM_FORMAT_C8`——四个 plane 都能收 C8 的 fb,驱动自动
   置 ATT_CTRL_REG0 的 palette 工作模式位;
2. 调色板上传接口 `/sys/kernel/debe_palette/palette`(1024 字节二进制
   文件),与 DRM master 无关,任何时刻可写,写完立即生效。

硬件只有**一份**调色板 SRAM(DEBE 偏移 0x1000-0x13FF),四个 layer
共用;它和 HWC 电量 overlay(补丁 0022,pattern 0x1400 / HWC 专用小
调色板 0x1600)分段编址,互不干扰。

## 0. 快速验证(不写代码)

板上已装 `c8test`(buildroot 包 `epass-test`,和 drmtest 一起):

```sh
# 默认表:开机即有灰阶 ramp(A=FF,RGB 从 000000 到 FFFFFF)
xxd /sys/kernel/debe_palette/palette | head -3

/etc/init.d/S01app stop        # 释放 DRM master

c8test -m ramp     # C8 主层:灰阶 → 彩虹 → 单项红色游标扫过 256 索引
c8test -m anim     # framebuffer 静止、只旋转调色板,颜色流动=硬件查表
c8test -m alpha    # XRGB 彩条 + C8 overlay:前 128 索引全透明/后 128 半透;
                   # 末尾自动验证"缩放 C8 被拒"
```

## 1. 调色板 sysfs 接口

```
/sys/kernel/debe_palette/palette    1024 字节,0644
```

- 内容:256 项 × 4 字节,**小端 ARGB8888**。第 i 项在偏移 `i*4`,
  文件内字节序即 `B, G, R, A`。C 里直接 `uint32_t pal[256]` 写
  `0xAARRGGBB` 然后整块 write 就是对的(小端机)。
- 支持部分写,但 offset 和长度都必须 **4 字节对齐**(整项),否则
  `-EINVAL`。改单项:`pwrite(fd, &entry, 4, idx * 4)`。
- 写入立即锁存(驱动写完 SRAM 就 commit),**不与 vsync 同步**:扫描
  中途换表,当前这一帧可能上半旧表下半新表。静态色板无所谓;逐帧调色板
  动画在意的话,把写入卡在 vblank 后(app 里 atomic commit 返回之后写)。
- 读返回驱动内的 shadow 副本(与硬件一致),驱动没 bind 时写返回
  `-ENODEV`。
- A=0x00 是全透明。想要"纯索引不透明"就把所有项 A 写 0xFF(默认灰阶
  表就是这样)。

## 2. DRM 侧用法

C8 就是一个普通的 fb 格式,建 fb 的两条路:

```c
/* dumb buffer:bpp 填 8,pitch = 宽 x 1 */
struct drm_mode_create_dumb creq = { .width = w, .height = h, .bpp = 8 };
drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);

/* legacy:depth=8, bpp=8 内核映射为 C8 */
drmModeAddFB(fd, w, h, 8, 8, creq.pitch, creq.handle, &fb_id);

/* 或 AddFB2(app 的 atomic 路线用这个) */
uint32_t handles[4] = { creq.handle }, pitches[4] = { creq.pitch }, offsets[4] = {0};
drmModeAddFB2(fd, w, h, DRM_FORMAT_C8, handles, pitches, offsets, &fb_id, 0);
```

之后 SetCrtc / SetPlane / atomic commit 都和别的格式一样;像素数据一
字节一索引,`map[y * pitch + x] = idx`。

modifier 只支持 LINEAR(C8 没有 tiled 变体)。

## 3. alpha 混合规则

和 ARGB8888 的行为完全对齐:

| plane 的 alpha 属性 | 生效的 alpha |
|---|---|
| 不设(OPAQUE,默认) | 调色板每项自己的 A(per-pixel 等效) |
| 设了(< 0xFFFF) | 全局 alpha 覆盖,调色板 A 被忽略 |

内核把 C8 一律按"带 alpha 的格式"参与 atomic_check 的 pipe 分配
(不然 per-entry alpha 进不了混合器)。副作用:**即使色板全不透明,
C8 层也占一个 alpha plane 名额**,而 suniv 上限是 2 个带 alpha 的
plane。叠三个以上带透明的层时注意预算(app 现有 NV12+ARGB 组合再加
一个 C8 overlay 是够的)。

## 4. 注意事项

1. **调色板全局唯一**。两个 C8 层同屏共用一张表,分段用索引(比如
   层 A 用 0-127、层 B 用 128-255)自行约定。
2. **不能缩放**。palette 查表在 DEBE 里做,DEFE(frontend)不认识
   索引格式;src 和 dst 尺寸不一致的 C8 commit 会被 `-EINVAL` 拒掉。
   要缩放就先自己转成真彩再走 DEFE。
3. **视频层不受影响**。走 DEFE 上屏的 NV12/tiled 层与 palette 模式
   无关,C8 只能是直接从 DRAM 取数的普通层;两者叠加没问题。
4. **和 HWC 电量 overlay 无冲突**。0022 的 HWC 用自己的 16 项小表
   (0x1600),不查这张 256 项大表;混合顺序也不变(HWC 恒在最上)。
5. **切格式无残留**。同一 plane 从 C8 换成 XRGB/NV12 或反过来,驱动
   在每次 commit 里置/清工作模式位,不需要用户态做任何清理;调色板
   内容跨格式切换保持不变(DRM 不碰那块 SRAM)。
6. **1/2/4bpp 硬件支持但没暴露**。5.4 的 DRM 只有 C8 这一个索引
   fourcc;更低 bpp 要等内核升级(C1/C2/C4 是 6.x 才有的),或者加
   自定义格式——目前不做。
