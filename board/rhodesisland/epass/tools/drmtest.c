// drmtest —— fbdev 时代 fbtest 的 DRM/libdrm 替身。
// 用 legacy KMS + dumb buffer 直接上屏,画一组对屏幕/时序有诊断意义的测试图案。
// 交叉编译见文件末尾注释。

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm.h>          // struct drm_mode_create_dumb / map_dumb / destroy_dumb + ioctl 号

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

// ---- dumb buffer 生命周期 --------------------------------------------------

static int fb_create(int fd, uint32_t w, uint32_t h, struct fb *out)
{
	struct drm_mode_create_dumb creq = { .width = w, .height = h, .bpp = 32 };
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		fprintf(stderr, "CREATE_DUMB: %s\n", strerror(errno));
		return -1;
	}
	out->handle = creq.handle;
	out->pitch  = creq.pitch;
	out->size   = creq.size;
	out->w = w;
	out->h = h;

	// XRGB8888，小端内存序 B,G,R,X。sun4i-drm 支持。
	if (drmModeAddFB(fd, w, h, 24, 32, out->pitch, out->handle, &out->fb_id)) {
		fprintf(stderr, "AddFB: %s\n", strerror(errno));
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
		out->map = NULL;
		return -1;
	}
	memset(out->map, 0, out->size);
	return 0;
}

static void fb_destroy(int fd, struct fb *f)
{
	if (f->map) munmap(f->map, f->size);
	if (f->fb_id) drmModeRmFB(fd, f->fb_id);
	if (f->handle) {
		struct drm_mode_destroy_dumb dreq = { .handle = f->handle };
		drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	}
	memset(f, 0, sizeof(*f));
}

static inline void px(struct fb *f, uint32_t x, uint32_t y, uint32_t argb)
{
	*(uint32_t *)(f->map + y * f->pitch + x * 4) = argb;
}

// ---- 测试图案 --------------------------------------------------------------

// 经典 SMPTE 竖彩条:白 黄 青 绿 品红 红 蓝
static void pat_bars(struct fb *f)
{
	static const uint32_t c[7] = {
		0xffffff, 0xffff00, 0x00ffff, 0x00ff00,
		0xff00ff, 0xff0000, 0x0000ff,
	};
	for (uint32_t y = 0; y < f->h; y++)
		for (uint32_t x = 0; x < f->w; x++)
			px(f, x, y, c[x * 7 / f->w]);
}

// 水平灰阶渐变,检查 gamma / 位深
static void pat_grad(struct fb *f)
{
	for (uint32_t x = 0; x < f->w; x++) {
		uint32_t v = x * 255 / (f->w - 1);
		uint32_t g = (v << 16) | (v << 8) | v;
		for (uint32_t y = 0; y < f->h; y++)
			px(f, x, y, g);
	}
}

// 对齐/过扫描测试:1px 白边框 + 每 32px 网格 + 四角标记
static void pat_grid(struct fb *f)
{
	for (uint32_t y = 0; y < f->h; y++)
		for (uint32_t x = 0; x < f->w; x++) {
			uint32_t v = 0x101010;
			int edge = (x == 0 || y == 0 || x == f->w - 1 || y == f->h - 1);
			if (edge) v = 0xffffff;
			else if (x % 32 == 0 || y % 32 == 0) v = 0x404040;
			px(f, x, y, v);
		}
	// 左上红、右上绿、左下蓝、右下白,10px 方块确认无镜像/翻转
	for (uint32_t y = 0; y < 10 && y < f->h; y++)
		for (uint32_t x = 0; x < 10 && x < f->w; x++) {
			px(f, x, y, 0xff0000);
			px(f, f->w - 1 - x, y, 0x00ff00);
			px(f, x, f->h - 1 - y, 0x0000ff);
			px(f, f->w - 1 - x, f->h - 1 - y, 0xffffff);
		}
}

// 棋盘格,像素级坏点/抖动检查
static void pat_checker(struct fb *f)
{
	for (uint32_t y = 0; y < f->h; y++)
		for (uint32_t x = 0; x < f->w; x++)
			px(f, x, y, ((x ^ y) & 1) ? 0xffffff : 0x000000);
}

static void pat_solid(struct fb *f, uint32_t argb)
{
	for (uint32_t y = 0; y < f->h; y++)
		for (uint32_t x = 0; x < f->w; x++)
			px(f, x, y, argb);
}

// ---- 主流程 ----------------------------------------------------------------

static drmModeConnector *find_connector(int fd, drmModeRes *res)
{
	for (int i = 0; i < res->count_connectors; i++) {
		drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
		if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0)
			return c;
		if (c) drmModeFreeConnector(c);
	}
	return NULL;
}

// connector 通过 encoder 找到可用的 crtc
static uint32_t find_crtc(int fd, drmModeRes *res, drmModeConnector *conn)
{
	if (conn->encoder_id) {
		drmModeEncoder *e = drmModeGetEncoder(fd, conn->encoder_id);
		if (e) {
			uint32_t id = e->crtc_id;
			drmModeFreeEncoder(e);
			if (id) return id;
		}
	}
	for (int i = 0; i < conn->count_encoders; i++) {
		drmModeEncoder *e = drmModeGetEncoder(fd, conn->encoders[i]);
		if (!e) continue;
		for (int j = 0; j < res->count_crtcs; j++)
			if (e->possible_crtcs & (1 << j)) {
				uint32_t id = res->crtcs[j];
				drmModeFreeEncoder(e);
				return id;
			}
		drmModeFreeEncoder(e);
	}
	return 0;
}

static void usage(const char *p)
{
	fprintf(stderr,
		"用法: %s [-d 设备] [-p 图案] [-t 秒] [-1]\n"
		"  -d  DRM 设备 (默认 /dev/dri/card0)\n"
		"  -p  bars|grad|grid|checker|red|green|blue|white|black|all (默认 all)\n"
		"  -t  每个图案停留秒数 (默认 3)\n"
		"  -1  只画一次不循环\n", p);
}

int main(int argc, char **argv)
{
	const char *dev = "/dev/dri/card0";
	const char *pat = "all";
	int secs = 3, once = 0, opt;

	while ((opt = getopt(argc, argv, "d:p:t:1h")) != -1) {
		switch (opt) {
		case 'd': dev = optarg; break;
		case 'p': pat = optarg; break;
		case 't': secs = atoi(optarg); break;
		case '1': once = 1; break;
		default: usage(argv[0]); return opt == 'h' ? 0 : 2;
		}
	}

	signal(SIGINT, on_sigint);
	signal(SIGTERM, on_sigint);

	int fd = open(dev, O_RDWR | O_CLOEXEC);
	if (fd < 0) { fprintf(stderr, "open %s: %s\n", dev, strerror(errno)); return 1; }

	drmModeRes *res = drmModeGetResources(fd);
	if (!res) { fprintf(stderr, "GetResources: %s (是 KMS 设备吗?)\n", strerror(errno)); close(fd); return 1; }

	drmModeConnector *conn = find_connector(fd, res);
	if (!conn) { fprintf(stderr, "没有已连接且带 mode 的 connector\n"); goto out_res; }

	drmModeModeInfo mode = conn->modes[0];  // [0] 通常是 preferred/native
	uint32_t crtc_id = find_crtc(fd, res, conn);
	if (!crtc_id) { fprintf(stderr, "找不到可用 CRTC\n"); goto out_conn; }

	printf("设备 %s | connector %u | CRTC %u | %s %dx%d@%d\n",
	       dev, conn->connector_id, crtc_id, mode.name,
	       mode.hdisplay, mode.vdisplay, mode.vrefresh);

	drmModeCrtc *saved = drmModeGetCrtc(fd, crtc_id);  // 退出时恢复

	struct fb f = {0};
	if (fb_create(fd, mode.hdisplay, mode.vdisplay, &f) < 0) goto out_saved;

	if (drmModeSetCrtc(fd, crtc_id, f.fb_id, 0, 0, &conn->connector_id, 1, &mode)) {
		fprintf(stderr, "SetCrtc: %s\n", strerror(errno));
		goto out_fb;
	}

	const char *seq[] = { "bars", "grad", "grid", "checker",
			      "red", "green", "blue", "white", "black" };
	int n = sizeof(seq) / sizeof(seq[0]);
	int all = strcmp(pat, "all") == 0;

	do {
		for (int i = 0; i < n && !g_stop; i++) {
			const char *p = all ? seq[i] : pat;

			if      (!strcmp(p, "bars"))    pat_bars(&f);
			else if (!strcmp(p, "grad"))    pat_grad(&f);
			else if (!strcmp(p, "grid"))    pat_grid(&f);
			else if (!strcmp(p, "checker")) pat_checker(&f);
			else if (!strcmp(p, "red"))     pat_solid(&f, 0xff0000);
			else if (!strcmp(p, "green"))   pat_solid(&f, 0x00ff00);
			else if (!strcmp(p, "blue"))    pat_solid(&f, 0x0000ff);
			else if (!strcmp(p, "white"))   pat_solid(&f, 0xffffff);
			else if (!strcmp(p, "black"))   pat_solid(&f, 0x000000);
			else { fprintf(stderr, "未知图案: %s\n", p); goto out_fb; }

			printf("  显示: %s\n", p);
			for (int t = 0; t < secs && !g_stop; t++) sleep(1);

			if (!all) break;  // 单图案:画一次后直接进入循环判定
		}
	} while (!once && !g_stop);

out_fb:
	if (saved)
		drmModeSetCrtc(fd, saved->crtc_id, saved->buffer_id, saved->x, saved->y,
			       &conn->connector_id, 1, &saved->mode);
	fb_destroy(fd, &f);
out_saved:
	if (saved) drmModeFreeCrtc(saved);
out_conn:
	drmModeFreeConnector(conn);
out_res:
	drmModeFreeResources(res);
	close(fd);
	return 0;
}

/*
 * Buildroot 包: package/epass-test (BR2_PACKAGE_EPASS_TEST)
 * 装到目标机 /usr/bin/drmtest
 *
 * 手工交叉编译 (在 buildroot 根目录):
 *   CC=output/host/bin/arm-buildroot-linux-musleabi-gcc
 *   SYS=output/staging
 *   $CC --sysroot=$SYS -O2 -Wall \
 *       -I$SYS/usr/include/libdrm \
 *       board/rhodesisland/epass/tools/drmtest.c \
 *       -ldrm -o drmtest
 */
