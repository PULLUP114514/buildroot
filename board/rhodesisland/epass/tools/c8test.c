// c8test —— DEBE 图层调色板(C8/index)模式测试。
// 依赖内核 patch 0029: DRM_FORMAT_C8 + /sys/kernel/debe_palette/palette。
// 交叉编译见文件末尾注释。

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm.h>
#include <drm_fourcc.h>

#define PALETTE_PATH "/sys/kernel/debe_palette/palette"

struct fb {
	uint32_t fb_id;
	uint32_t handle;
	uint32_t pitch;
	uint64_t size;
	uint8_t *map;
	uint32_t w, h;
};

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int s) { (void)s; g_stop = 1; }

// ---- dumb buffer ----------------------------------------------------------

static int fb_create(int fd, uint32_t w, uint32_t h, int bpp, struct fb *out)
{
	struct drm_mode_create_dumb creq = { .width = w, .height = h, .bpp = bpp };
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		fprintf(stderr, "CREATE_DUMB(%d): %s\n", bpp, strerror(errno));
		return -1;
	}
	out->handle = creq.handle;
	out->pitch  = creq.pitch;
	out->size   = creq.size;
	out->w = w;
	out->h = h;

	// depth=8/bpp=8 经 drm_mode_legacy_fb_format 映射为 DRM_FORMAT_C8
	int depth = (bpp == 8) ? 8 : 24;
	if (drmModeAddFB(fd, w, h, depth, bpp, out->pitch, out->handle, &out->fb_id)) {
		fprintf(stderr, "AddFB(depth=%d,bpp=%d): %s\n", depth, bpp, strerror(errno));
		return -1;
	}

	struct drm_mode_map_dumb mreq = { .handle = out->handle };
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
		fprintf(stderr, "MAP_DUMB: %s\n", strerror(errno));
		return -1;
	}
	out->map = mmap(NULL, out->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	if (out->map == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static void fb_destroy(int fd, struct fb *f)
{
	if (f->map && f->map != MAP_FAILED) munmap(f->map, f->size);
	if (f->fb_id) drmModeRmFB(fd, f->fb_id);
	if (f->handle) {
		struct drm_mode_destroy_dumb dreq = { .handle = f->handle };
		drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	}
}

// ---- 调色板 ---------------------------------------------------------------

static int palette_write(const uint32_t *pal, size_t count, off_t entry_off)
{
	int fd = open(PALETTE_PATH, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", PALETTE_PATH, strerror(errno));
		return -1;
	}
	ssize_t n = pwrite(fd, pal, count * 4, entry_off * 4);
	if (n != (ssize_t)(count * 4))
		fprintf(stderr, "pwrite palette: %s\n", strerror(errno));
	close(fd);
	return n == (ssize_t)(count * 4) ? 0 : -1;
}

// 彩虹表: phase 0-255 整表旋转色相,alpha 恒 0xff
static void pal_rainbow(uint32_t *pal, int phase)
{
	for (int i = 0; i < 256; i++) {
		int h = (i + phase) & 0xff;          // 0-255 映射到 0-360°
		int seg = h / 43, rem = (h % 43) * 6;
		int q = 255 - rem, t = rem;
		if (q < 0) q = 0;
		if (t > 255) t = 255;
		uint8_t r, g, b;
		switch (seg) {
		case 0: r = 255; g = t;   b = 0;   break;
		case 1: r = q;   g = 255; b = 0;   break;
		case 2: r = 0;   g = 255; b = t;   break;
		case 3: r = 0;   g = q;   b = 255; break;
		case 4: r = t;   g = 0;   b = 255; break;
		default:r = 255; g = 0;   b = q;   break;
		}
		pal[i] = 0xff000000u | (r << 16) | (g << 8) | b;
	}
}

static void pal_gray(uint32_t *pal)
{
	for (int i = 0; i < 256; i++)
		pal[i] = 0xff000000u | (i * 0x010101u);
}

// alpha 测试表: 前 128 项全透明,后 128 项半透明彩虹
static void pal_alpha_test(uint32_t *pal)
{
	pal_rainbow(pal, 0);
	for (int i = 0; i < 128; i++)
		pal[i] &= 0x00ffffffu;
	for (int i = 128; i < 256; i++)
		pal[i] = (pal[i] & 0x00ffffffu) | 0x80000000u;
}

// ---- 图案 -----------------------------------------------------------------

// 竖向 256 列 index ramp + 顶部 16x16 色块阵(每块一个 index)
static void pat_index(struct fb *f)
{
	uint32_t block = f->w / 16;
	if (block > f->h / 16) block = f->h / 16;
	uint32_t grid_h = block * 16;

	for (uint32_t y = 0; y < f->h; y++) {
		uint8_t *row = f->map + y * f->pitch;
		if (y < grid_h) {
			for (uint32_t x = 0; x < f->w; x++) {
				uint32_t bx = x / block, by = y / block;
				row[x] = (bx < 16) ? (by * 16 + bx) : 0;
			}
		} else {
			for (uint32_t x = 0; x < f->w; x++)
				row[x] = x * 256 / f->w;
		}
	}
}

static void pat_bars32(struct fb *f)
{
	static const uint32_t c[7] = {
		0xffffff, 0xffff00, 0x00ffff, 0x00ff00,
		0xff00ff, 0xff0000, 0x0000ff };
	for (uint32_t y = 0; y < f->h; y++) {
		uint32_t *row = (uint32_t *)(f->map + y * f->pitch);
		for (uint32_t x = 0; x < f->w; x++)
			row[x] = c[x * 7 / f->w];
	}
}

// ---- KMS 查找 -------------------------------------------------------------

static drmModeConnector *find_connector(int fd, drmModeRes *res)
{
	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
		if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes)
			return c;
		if (c) drmModeFreeConnector(c);
	}
	return NULL;
}

static uint32_t find_crtc(int fd, drmModeRes *res, drmModeConnector *conn)
{
	for (int i = 0; i < conn->count_encoders; i++) {
		drmModeEncoder *e = drmModeGetEncoder(fd, conn->encoders[i]);
		if (!e) continue;
		for (int j = 0; j < res->count_crtcs; j++)
			if (e->possible_crtcs & (1u << j)) {
				uint32_t id = res->crtcs[j];
				drmModeFreeEncoder(e);
				return id;
			}
		drmModeFreeEncoder(e);
	}
	return 0;
}

// 找一个支持 C8 且当前空闲(没绑 fb)的 plane —— SetCrtc 之后主层已被占,
// 剩下 fb_id==0 的即 overlay
static uint32_t find_c8_overlay(int fd, drmModeRes *res, uint32_t crtc_id)
{
	int crtc_idx = -1;
	for (int i = 0; i < res->count_crtcs; i++)
		if (res->crtcs[i] == crtc_id) crtc_idx = i;

	drmModePlaneRes *pres = drmModeGetPlaneResources(fd);
	if (!pres) return 0;

	uint32_t found = 0;
	for (uint32_t i = 0; i < pres->count_planes && !found; i++) {
		drmModePlane *p = drmModeGetPlane(fd, pres->planes[i]);
		if (!p) continue;
		if ((p->possible_crtcs & (1u << crtc_idx)) && p->fb_id == 0) {
			for (uint32_t j = 0; j < p->count_formats; j++)
				if (p->formats[j] == DRM_FORMAT_C8) {
					found = p->plane_id;
					break;
				}
		}
		drmModeFreePlane(p);
	}
	drmModeFreePlaneResources(pres);
	return found;
}

// ---- 测试模式 -------------------------------------------------------------

static int mode_ramp(int fd, uint32_t crtc_id, drmModeConnector *conn,
		     drmModeModeInfo *mode, int secs, int anim)
{
	struct fb f = {0};
	uint32_t pal[256];

	if (fb_create(fd, mode->hdisplay, mode->vdisplay, 8, &f) < 0)
		return -1;
	pat_index(&f);

	pal_gray(pal);
	palette_write(pal, 256, 0);

	if (drmModeSetCrtc(fd, crtc_id, f.fb_id, 0, 0, &conn->connector_id, 1, mode)) {
		fprintf(stderr, "SetCrtc(C8): %s\n", strerror(errno));
		fb_destroy(fd, &f);
		return -1;
	}
	printf("C8 上屏: 灰阶 ramp,%d 秒\n", secs);
	for (int t = 0; t < secs && !g_stop; t++) sleep(1);

	if (anim) {
		// framebuffer 完全不动,只旋转调色板 —— 颜色变即证明硬件查表
		printf("调色板动画 (framebuffer 静止),Ctrl-C 退出\n");
		int phase = 0;
		while (!g_stop) {
			pal_rainbow(pal, phase++ & 0xff);
			palette_write(pal, 256, 0);
			usleep(33000);
		}
	} else {
		pal_rainbow(pal, 0);
		palette_write(pal, 256, 0);
		printf("彩虹表,%d 秒\n", secs);
		for (int t = 0; t < secs && !g_stop; t++) sleep(1);

		// 部分写: 单项改成红色游标扫过色块阵
		printf("单项部分写测试 (index 0-255 逐个变红)\n");
		for (int i = 0; i < 256 && !g_stop; i++) {
			uint32_t save = pal[i], red = 0xffff0000u;
			palette_write(&red, 1, i);
			usleep(20000);
			palette_write(&save, 1, i);
		}
	}

	fb_destroy(fd, &f);
	return 0;
}

static int mode_alpha(int fd, drmModeRes *res, uint32_t crtc_id,
		      drmModeConnector *conn, drmModeModeInfo *mode, int secs)
{
	struct fb bg = {0}, ov = {0};
	uint32_t pal[256];
	int ret = -1;

	if (fb_create(fd, mode->hdisplay, mode->vdisplay, 32, &bg) < 0)
		return -1;
	pat_bars32(&bg);
	if (drmModeSetCrtc(fd, crtc_id, bg.fb_id, 0, 0, &conn->connector_id, 1, mode)) {
		fprintf(stderr, "SetCrtc(XRGB): %s\n", strerror(errno));
		goto out_bg;
	}

	uint32_t plane_id = find_c8_overlay(fd, res, crtc_id);
	if (!plane_id) {
		fprintf(stderr, "找不到支持 C8 的空闲 overlay plane\n");
		goto out_bg;
	}
	printf("overlay plane %u\n", plane_id);

	if (fb_create(fd, mode->hdisplay, mode->vdisplay, 8, &ov) < 0)
		goto out_bg;
	pat_index(&ov);

	pal_alpha_test(pal);
	palette_write(pal, 256, 0);

	// 上半屏应透出彩条(index<128 全透明),下半屏半透混合
	if (drmModeSetPlane(fd, plane_id, crtc_id, ov.fb_id, 0,
			    0, 0, ov.w, ov.h,
			    0, 0, ov.w << 16, ov.h << 16)) {
		fprintf(stderr, "SetPlane(C8): %s\n", strerror(errno));
		goto out_ov;
	}
	printf("alpha 混合: 前 128 index 全透明 / 后 128 半透,%d 秒\n", secs);
	for (int t = 0; t < secs && !g_stop; t++) sleep(1);

	// 负面测试: 缩放 C8 应被拒 (frontend 不支持 palette)
	if (drmModeSetPlane(fd, plane_id, crtc_id, ov.fb_id, 0,
			    0, 0, ov.w, ov.h,
			    0, 0, (ov.w / 2) << 16, (ov.h / 2) << 16) == 0)
		fprintf(stderr, "警告: 缩放 C8 竟然成功了,应当被拒\n");
	else
		printf("缩放 C8 被拒 (%s) —— 符合预期\n", strerror(errno));

	drmModeSetPlane(fd, plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	ret = 0;
out_ov:
	fb_destroy(fd, &ov);
out_bg:
	fb_destroy(fd, &bg);
	return ret;
}

static void usage(const char *p)
{
	fprintf(stderr,
		"用法: %s [-d 设备] [-m 模式] [-t 秒]\n"
		"  -d  DRM 设备 (默认 /dev/dri/card0)\n"
		"  -m  ramp|anim|alpha (默认 ramp)\n"
		"      ramp:  C8 主层 + 灰阶/彩虹表 + 单项部分写\n"
		"      anim:  C8 主层,循环旋转调色板 (framebuffer 不动)\n"
		"      alpha: XRGB 彩条主层 + C8 overlay,验证 per-entry alpha 与缩放拒绝\n"
		"  -t  各阶段停留秒数 (默认 3)\n", p);
}

int main(int argc, char **argv)
{
	const char *dev = "/dev/dri/card0";
	const char *m = "ramp";
	int secs = 3, opt;

	while ((opt = getopt(argc, argv, "d:m:t:h")) != -1) {
		switch (opt) {
		case 'd': dev = optarg; break;
		case 'm': m = optarg; break;
		case 't': secs = atoi(optarg); break;
		default: usage(argv[0]); return opt == 'h' ? 0 : 2;
		}
	}

	signal(SIGINT, on_sigint);
	signal(SIGTERM, on_sigint);

	int fd = open(dev, O_RDWR | O_CLOEXEC);
	if (fd < 0) { fprintf(stderr, "open %s: %s\n", dev, strerror(errno)); return 1; }

	drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

	drmModeRes *res = drmModeGetResources(fd);
	if (!res) { fprintf(stderr, "GetResources: %s (是 KMS 设备吗?)\n", strerror(errno)); close(fd); return 1; }

	drmModeConnector *conn = find_connector(fd, res);
	if (!conn) { fprintf(stderr, "没有已连接且带 mode 的 connector\n"); goto out_res; }

	drmModeModeInfo mode = conn->modes[0];
	uint32_t crtc_id = find_crtc(fd, res, conn);
	if (!crtc_id) { fprintf(stderr, "找不到可用 CRTC\n"); goto out_conn; }

	printf("设备 %s | connector %u | CRTC %u | %s %dx%d@%d\n",
	       dev, conn->connector_id, crtc_id, mode.name,
	       mode.hdisplay, mode.vdisplay, mode.vrefresh);

	drmModeCrtc *saved = drmModeGetCrtc(fd, crtc_id);

	int ret;
	if (!strcmp(m, "ramp"))
		ret = mode_ramp(fd, crtc_id, conn, &mode, secs, 0);
	else if (!strcmp(m, "anim"))
		ret = mode_ramp(fd, crtc_id, conn, &mode, secs, 1);
	else if (!strcmp(m, "alpha"))
		ret = mode_alpha(fd, res, crtc_id, conn, &mode, secs);
	else { usage(argv[0]); ret = -1; }

	if (saved) {
		drmModeSetCrtc(fd, saved->crtc_id, saved->buffer_id, saved->x, saved->y,
			       &conn->connector_id, 1, &saved->mode);
		drmModeFreeCrtc(saved);
	}
	(void)ret;
out_conn:
	drmModeFreeConnector(conn);
out_res:
	drmModeFreeResources(res);
	close(fd);
	return 0;
}

/*
 * Buildroot 包: package/epass-test (BR2_PACKAGE_EPASS_TEST)
 * 装到目标机 /usr/bin/c8test
 *
 * 手工交叉编译 (在 buildroot 根目录):
 *   CC=output/host/bin/arm-buildroot-linux-musleabi-gcc
 *   SYS=output/staging
 *   $CC --sysroot=$SYS -O2 -Wall \
 *       -I$SYS/usr/include/libdrm \
 *       board/rhodesisland/epass/tools/c8test.c \
 *       -ldrm -o c8test
 */
