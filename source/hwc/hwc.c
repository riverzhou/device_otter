/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/omapfb.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <stdbool.h>

#include <cutils/properties.h>
#include <cutils/log.h>
#include <cutils/native_handle.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <EGL/egl.h>
#include <hardware_legacy/uevent.h>
#include <utils/Timers.h>

#include <system/graphics.h>
#include "linux/bltsville.h"

#define MAX_HWC_LAYERS 32

#define ASPECT_RATIO_TOLERANCE 0.02f

#define min(a, b) ( { typeof(a) __a = (a), __b = (b); __a < __b ? __a : __b; } )
#define max(a, b) ( { typeof(a) __a = (a), __b = (b); __a > __b ? __a : __b; } )
#define swap(a, b) do { typeof(a) __a = (a); (a) = (b); (b) = __a; } while (0)

#define WIDTH(rect) ((rect).right - (rect).left)
#define HEIGHT(rect) ((rect).bottom - (rect).top)

#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))

#include <video/dsscomp.h>
#include "video/omap_hwc.h"

#include "hal_public.h"

#define MAX_HW_OVERLAYS 4
#define NUM_NONSCALING_OVERLAYS 1

enum bltpolicy {
    BLTPOLICY_DISABLED = 0,
    BLTPOLICY_DEFAULT = 1,    /* Default blit policy */
    BLTPOLICY_ALL,            /* Test mode to attempt to blit all */
};

struct omap4_hwc_module {
    hwc_module_t base;

    IMG_framebuffer_device_public_t *fb_dev;
};
typedef struct omap4_hwc_module omap4_hwc_module_t;

struct counts {
    unsigned int possible_overlay_layers;
    unsigned int composited_layers;
    unsigned int scaled_layers;
    unsigned int RGB;
    unsigned int BGR;
    unsigned int NV12;
    unsigned int protected;
    unsigned int max_hw_overlays;
    unsigned int max_scaling_overlays;
    unsigned int mem;
};

struct omap4_hwc_device {
    /* static data */
    hwc_composer_device_t base;
    hwc_procs_t *procs;
    pthread_t hdmi_thread;
    pthread_mutex_t lock;

    IMG_framebuffer_device_public_t *fb_dev;
    struct dsscomp_display_info fb_dis;
    int fb_fd;                  /* file descriptor for /dev/fb0 */
    int dsscomp_fd;             /* file descriptor for /dev/dsscomp */
    int pipe_fds[2];            /* pipe to event thread */

    int flags_rgb_order;
    int flags_nv12_only;
    float upscaled_nv12_limit;

    int force_sgx;
    int idle;

    float primary_m[2][3];          /* internal transformation matrix */
    int primary_transform;
    int primary_rotation;
    hwc_rect_t primary_region;

    buffer_handle_t *buffers;
    int use_sgx;
    int swap_rb;
    unsigned int post2_layers; /* Buffers used with DSS pipes*/
    int last_int_ovls;

    enum bltpolicy blt_policy;

    struct omap_hwc_data comp_data; /* This is a kernel data structure */
    struct counts stats;
};
typedef struct omap4_hwc_device omap4_hwc_device_t;

#define HAL_FMT(f) ((f) == HAL_PIXEL_FORMAT_TI_NV12 ? "NV12" : \
                    (f) == HAL_PIXEL_FORMAT_TI_NV12_1D ? "NV12" : \
                    (f) == HAL_PIXEL_FORMAT_YV12 ? "YV12" : \
                    (f) == HAL_PIXEL_FORMAT_BGRX_8888 ? "xRGB32" : \
                    (f) == HAL_PIXEL_FORMAT_RGBX_8888 ? "xBGR32" : \
                    (f) == HAL_PIXEL_FORMAT_BGRA_8888 ? "ARGB32" : \
                    (f) == HAL_PIXEL_FORMAT_RGBA_8888 ? "ABGR32" : \
                    (f) == HAL_PIXEL_FORMAT_RGB_565 ? "RGB565" : "??")

#define DSS_FMT(f) ((f) == OMAP_DSS_COLOR_NV12 ? "NV12" : \
                    (f) == OMAP_DSS_COLOR_RGB24U ? "xRGB32" : \
                    (f) == OMAP_DSS_COLOR_ARGB32 ? "ARGB32" : \
                    (f) == OMAP_DSS_COLOR_RGB16 ? "RGB565" : "??")

static int debug = 1;
static int debugpost2 = 1;
static int debugblt = 1;

static void showfps(void)
{
    static int framecount = 0;
    static int lastframecount = 0;
    static nsecs_t lastfpstime = 0;
    static float fps = 0;
    char value[PROPERTY_VALUE_MAX];

    property_get("debug.hwc.showfps", value, "0");
    if (!atoi(value)) {
        return;
    }

    framecount++;
    if (!(framecount & 0x7)) {
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        nsecs_t diff = now - lastfpstime;
        fps = ((framecount - lastframecount) * (float)(s2ns(1))) / diff;
        lastfpstime = now;
        lastframecount = framecount;
        ALOGI("%d Frames, %f FPS", framecount, fps);
    }
}

static void dump_layer(hwc_layer_t const* l)
{
    ALOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
         l->compositionType, l->flags, l->handle, l->transform, l->blending,
         l->sourceCrop.left,
         l->sourceCrop.top,
         l->sourceCrop.right,
         l->sourceCrop.bottom,
         l->displayFrame.left,
         l->displayFrame.top,
         l->displayFrame.right,
         l->displayFrame.bottom);
}

static void dump_dsscomp(struct dsscomp_setup_dispc_data *d)
{
    unsigned i;

    ALOGD("[%08x] set: %c%c%c %d ovls\n",
         d->sync_id,
         (d->mode & DSSCOMP_SETUP_MODE_APPLY) ? 'A' : '-',
         (d->mode & DSSCOMP_SETUP_MODE_DISPLAY) ? 'D' : '-',
         (d->mode & DSSCOMP_SETUP_MODE_CAPTURE) ? 'C' : '-',
         d->num_ovls);

    for (i = 0; i < d->num_mgrs; i++) {
        struct dss2_mgr_info *mi = &d->mgrs[i];
        ALOGD(" (dis%d alpha=%d col=%08x ilace=%d)\n",
             mi->ix,
             mi->alpha_blending, mi->default_color,
             mi->interlaced);
    }

    for (i = 0; i < d->num_ovls; i++) {
        struct dss2_ovl_info *oi = &d->ovls[i];
        struct dss2_ovl_cfg *c = &oi->cfg;
        if (c->zonly)
            ALOGD("ovl%d(%s z%d)\n",
                 c->ix, c->enabled ? "ON" : "off", c->zorder);
        else
            ALOGD("ovl%d(%s z%d %s%s *%d%% %d*%d:%d,%d+%d,%d rot%d%s => %d,%d+%d,%d %p/%p|%d)\n",
                 c->ix, c->enabled ? "ON" : "off", c->zorder, DSS_FMT(c->color_mode),
                 c->pre_mult_alpha ? " premult" : "",
                 (c->global_alpha * 100 + 128) / 255,
                 c->width, c->height, c->crop.x, c->crop.y,
                 c->crop.w, c->crop.h,
                 c->rotation, c->mirror ? "+mir" : "",
                 c->win.x, c->win.y, c->win.w, c->win.h,
                 (void *) oi->ba, (void *) oi->uv, c->stride);
    }
}

struct dump_buf {
    char *buf;
    int buf_len;
    int len;
};

static void dump_printf(struct dump_buf *buf, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    buf->len += vsnprintf(buf->buf + buf->len, buf->buf_len - buf->len, fmt, ap);
    va_end(ap);
}

static void dump_set_info(omap4_hwc_device_t *hwc_dev, hwc_layer_list_t* list)
{
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->comp_data.dsscomp_data;
    char logbuf[1024];
    struct dump_buf log = {
        .buf = logbuf,
        .buf_len = sizeof(logbuf),
    };
    unsigned int i;

    dump_printf(&log, "set H{");
    for (i = 0; list && i < list->numHwLayers; i++) {
        if (i)
            dump_printf(&log, " ");
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;
        dump_printf(&log, "%p:%s,", handle, layer->compositionType == HWC_OVERLAY ? "DSS" : "SGX");
        if ((layer->flags & HWC_SKIP_LAYER) || !handle) {
            dump_printf(&log, "SKIP");
            continue;
        }
        if (layer->flags & HWC_HINT_CLEAR_FB)
            dump_printf(&log, "CLR,");
        dump_printf(&log, "%d*%d(%s)", handle->iWidth, handle->iHeight, HAL_FMT(handle->iFormat));
        if (layer->transform)
            dump_printf(&log, "~%d", layer->transform);
    }
    dump_printf(&log, "} D{");
    for (i = 0; i < dsscomp->num_ovls; i++) {
        if (i)
            dump_printf(&log, " ");
        dump_printf(&log, "%d=", dsscomp->ovls[i].cfg.ix);
        if (dsscomp->ovls[i].cfg.enabled)
            dump_printf(&log, "%08x:%d*%d,%s",
                        dsscomp->ovls[i].ba,
                        dsscomp->ovls[i].cfg.width,
                        dsscomp->ovls[i].cfg.height,
                        DSS_FMT(dsscomp->ovls[i].cfg.color_mode));
        else
            dump_printf(&log, "-");
    }
    dump_printf(&log, "} L{");
    for (i = 0; i < hwc_dev->post2_layers; i++) {
        if (i)
            dump_printf(&log, " ");
        dump_printf(&log, "%p", hwc_dev->buffers[i]);
    }
    dump_printf(&log, "}%s\n", hwc_dev->use_sgx ? " swap" : "");

    ALOGD("%s", log.buf);
}

static int sync_id = 0;

static int omap4_hwc_is_valid_format(int format)
{
    switch(format) {
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_BGRX_8888:
    case HAL_PIXEL_FORMAT_TI_NV12:
    case HAL_PIXEL_FORMAT_TI_NV12_1D:
        return 1;

    default:
        return 0;
    }
}

static int scaled(hwc_layer_t *layer)
{
    int w = WIDTH(layer->sourceCrop);
    int h = HEIGHT(layer->sourceCrop);

    if (layer->transform & HWC_TRANSFORM_ROT_90)
        swap(w, h);

    return WIDTH(layer->displayFrame) != w || HEIGHT(layer->displayFrame) != h ;
}

static int is_protected(hwc_layer_t *layer)
{
    IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

    return (handle->usage & GRALLOC_USAGE_PROTECTED);
}

#define is_BLENDED(layer) ((layer)->blending != HWC_BLENDING_NONE)

static int is_RGB(IMG_native_handle_t *handle)
{
    switch(handle->iFormat)
    {
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_BGRX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
        return 1;
    default:
        return 0;
    }
}
static int get_rgb_bpp(IMG_native_handle_t *handle)
{
    switch(handle->iFormat)
    {
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_BGRX_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return 32;
    case HAL_PIXEL_FORMAT_RGB_565:
        return 16;
    default:
        return 0;
    }
}

static int is_BGR_format(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return 1;
    default:
        return 0;
    }
}

static int is_BGR(IMG_native_handle_t *handle)
{
    return is_BGR_format(handle->iFormat);
}

static int is_NV12(IMG_native_handle_t *handle)
{
    switch(handle->iFormat)
    {
    case HAL_PIXEL_FORMAT_TI_NV12:
    case HAL_PIXEL_FORMAT_TI_NV12_1D:
        return 1;
    default:
        return 0;
    }
}

static int is_upscaled_NV12(omap4_hwc_device_t *hwc_dev, hwc_layer_t *layer)
{
    if (!layer)
        return 0;

    IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;
    if (!is_NV12(handle))
        return 0;

    int w = WIDTH(layer->sourceCrop);
    int h = HEIGHT(layer->sourceCrop);

    if (layer->transform & HWC_TRANSFORM_ROT_90)
        swap(w, h);

    return (WIDTH(layer->displayFrame) >= w * hwc_dev->upscaled_nv12_limit ||
            HEIGHT(layer->displayFrame) >= h * hwc_dev->upscaled_nv12_limit);
}

static unsigned int mem1d(IMG_native_handle_t *handle)
{
    if (handle == NULL || is_NV12(handle))
        return 0;

    int bpp = handle->iFormat == HAL_PIXEL_FORMAT_RGB_565 ? 2 : 4;
    int stride = ALIGN(handle->iWidth, HW_ALIGN) * bpp;
    return stride * handle->iHeight;
}

static void
omap4_hwc_setup_layer_base(struct dss2_ovl_cfg *oc, int index, int format, int blended, int width, int height)
{
    unsigned int bits_per_pixel;

    /* YUV2RGB conversion */
    const struct omap_dss_cconv_coefs ctbl_bt601_5 = {
        298,  409,    0,  298, -208, -100,  298,    0,  517, 0,
    };

    /* convert color format */
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        oc->color_mode = OMAP_DSS_COLOR_ARGB32;
        bits_per_pixel = 32;
        if (blended)
                break;

    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRX_8888:
        oc->color_mode = OMAP_DSS_COLOR_RGB24U;
        bits_per_pixel = 32;
        break;

    case HAL_PIXEL_FORMAT_RGB_565:
        oc->color_mode = OMAP_DSS_COLOR_RGB16;
        bits_per_pixel = 16;
        break;

    case HAL_PIXEL_FORMAT_TI_NV12:
    case HAL_PIXEL_FORMAT_TI_NV12_1D:
        oc->color_mode = OMAP_DSS_COLOR_NV12;
        bits_per_pixel = 8;
        oc->cconv = ctbl_bt601_5;
        break;

    default:
        /* Should have been filtered out */
        ALOGV("Unsupported pixel format");
        return;
    }

    oc->width = width;
    oc->height = height;
    oc->stride = ALIGN(width, HW_ALIGN) * bits_per_pixel / 8;

    oc->enabled = 1;
    oc->global_alpha = 255;
    oc->zorder = index;
    oc->ix = 0;

    /* defaults for SGX framebuffer renders */
    oc->crop.w = oc->win.w = width;
    oc->crop.h = oc->win.h = height;

    /* for now interlacing and vc1 info is not supplied */
    oc->ilace = OMAP_DSS_ILACE_NONE;
    oc->vc1.enable = 0;
}

static void
omap4_hwc_setup_layer(omap4_hwc_device_t *hwc_dev, struct dss2_ovl_info *ovl,
                      hwc_layer_t *layer, int index,
                      int format, int width, int height)
{
    struct dss2_ovl_cfg *oc = &ovl->cfg;

    //dump_layer(layer);

    omap4_hwc_setup_layer_base(oc, index, format, is_BLENDED(layer), width, height);

    /* convert transformation - assuming 0-set config */
    if (layer->transform & HWC_TRANSFORM_FLIP_H)
        oc->mirror = 1;
    if (layer->transform & HWC_TRANSFORM_FLIP_V) {
        oc->rotation = 2;
        oc->mirror = !oc->mirror;
    }
    if (layer->transform & HWC_TRANSFORM_ROT_90) {
        oc->rotation += oc->mirror ? -1 : 1;
        oc->rotation &= 3;
    }

    oc->pre_mult_alpha = layer->blending == HWC_BLENDING_PREMULT;

    /* display position */
    oc->win.x = layer->displayFrame.left;
    oc->win.y = layer->displayFrame.top;
    oc->win.w = WIDTH(layer->displayFrame);
    oc->win.h = HEIGHT(layer->displayFrame);

    /* crop */
    oc->crop.x = layer->sourceCrop.left;
    oc->crop.y = layer->sourceCrop.top;
    oc->crop.w = WIDTH(layer->sourceCrop);
    oc->crop.h = HEIGHT(layer->sourceCrop);
}

const float m_unit[2][3] = { { 1., 0., 0. }, { 0., 1., 0. } };

static inline void m_translate(float m[2][3], int dx, int dy)
{
    m[0][2] += dx;
    m[1][2] += dy;
}

static inline void m_scale1(float m[3], int from, int to)
{
    m[0] = m[0] * to / from;
    m[1] = m[1] * to / from;
    m[2] = m[2] * to / from;
}

static inline void m_scale(float m[2][3], int x_from, int x_to, int y_from, int y_to)
{
    m_scale1(m[0], x_from, x_to);
    m_scale1(m[1], y_from, y_to);
}

static void m_rotate(float m[2][3], int quarter_turns)
{
    if (quarter_turns & 2)
        m_scale(m, 1, -1, 1, -1);
    if (quarter_turns & 1) {
        int q;
        q = m[0][0]; m[0][0] = -m[1][0]; m[1][0] = q;
        q = m[0][1]; m[0][1] = -m[1][1]; m[1][1] = q;
        q = m[0][2]; m[0][2] = -m[1][2]; m[1][2] = q;
    }
}

static inline int m_round(float x)
{
    /* int truncates towards 0 */
    return (int) (x < 0 ? x - 0.5 : x + 0.5);
}

static int
crop_to_rect(struct dss2_ovl_cfg *cfg, struct hwc_rect vis_rect)
{
    struct {
        int xy[2];
        int wh[2];
    } crop, win;
    struct {
        int lt[2];
        int rb[2];
    } vis;
    win.xy[0] = cfg->win.x; win.xy[1] = cfg->win.y;
    win.wh[0] = cfg->win.w; win.wh[1] = cfg->win.h;
    crop.xy[0] = cfg->crop.x; crop.xy[1] = cfg->crop.y;
    crop.wh[0] = cfg->crop.w; crop.wh[1] = cfg->crop.h;
    vis.lt[0] = vis_rect.left; vis.lt[1] = vis_rect.top;
    vis.rb[0] = vis_rect.right; vis.rb[1] = vis_rect.bottom;

    int c, swap = cfg->rotation & 1;

    /* align crop window with display coordinates */
    if (swap)
        crop.xy[1] -= (crop.wh[1] = -crop.wh[1]);
    if (cfg->rotation & 2)
        crop.xy[!swap] -= (crop.wh[!swap] = -crop.wh[!swap]);
    if ((!cfg->mirror) ^ !(cfg->rotation & 2))
        crop.xy[swap] -= (crop.wh[swap] = -crop.wh[swap]);

    for (c = 0; c < 2; c++) {
        /* see if complete buffer is outside the vis or it is
          fully cropped or scaled to 0 */
        if (win.wh[c] <= 0 || vis.rb[c] <= vis.lt[c] ||
            win.xy[c] + win.wh[c] <= vis.lt[c] ||
            win.xy[c] >= vis.rb[c] ||
            !crop.wh[c ^ swap])
            return -ENOENT;

        /* crop left/top */
        if (win.xy[c] < vis.lt[c]) {
            /* correction term */
            int a = (vis.lt[c] - win.xy[c]) * crop.wh[c ^ swap] / win.wh[c];
            crop.xy[c ^ swap] += a;
            crop.wh[c ^ swap] -= a;
            win.wh[c] -= vis.lt[c] - win.xy[c];
            win.xy[c] = vis.lt[c];
        }
        /* crop right/bottom */
        if (win.xy[c] + win.wh[c] > vis.rb[c]) {
            crop.wh[c ^ swap] = crop.wh[c ^ swap] * (vis.rb[c] - win.xy[c]) / win.wh[c];
            win.wh[c] = vis.rb[c] - win.xy[c];
        }

        if (!crop.wh[c ^ swap] || !win.wh[c])
            return -ENOENT;
    }

    /* realign crop window to buffer coordinates */
    if (cfg->rotation & 2)
        crop.xy[!swap] -= (crop.wh[!swap] = -crop.wh[!swap]);
    if ((!cfg->mirror) ^ !(cfg->rotation & 2))
        crop.xy[swap] -= (crop.wh[swap] = -crop.wh[swap]);
    if (swap)
        crop.xy[1] -= (crop.wh[1] = -crop.wh[1]);

    cfg->win.x = win.xy[0]; cfg->win.y = win.xy[1];
    cfg->win.w = win.wh[0]; cfg->win.h = win.wh[1];
    cfg->crop.x = crop.xy[0]; cfg->crop.y = crop.xy[1];
    cfg->crop.w = crop.wh[0]; cfg->crop.h = crop.wh[1];

    return 0;
}

static void
omap4_hwc_apply_transform(float transform[2][3],struct dss2_ovl_cfg *oc)
{
    float x, y, w, h;

    /* display position */
    x = transform[0][0] * oc->win.x + transform[0][1] * oc->win.y + transform[0][2];
    y = transform[1][0] * oc->win.x + transform[1][1] * oc->win.y + transform[1][2];
    w = transform[0][0] * oc->win.w + transform[0][1] * oc->win.h;
    h = transform[1][0] * oc->win.w + transform[1][1] * oc->win.h;
    oc->win.x = m_round(w > 0 ? x : x + w);
    oc->win.y = m_round(h > 0 ? y : y + h);
    oc->win.w = m_round(w > 0 ? w : -w);
    oc->win.h = m_round(h > 0 ? h : -h);
}

static struct dsscomp_platform_info limits;

static void
omap4_hwc_adjust_primary_display_layer(omap4_hwc_device_t *hwc_dev, struct dss2_ovl_info *ovl)
{
    struct dss2_ovl_cfg *oc = &ovl->cfg;

    if (crop_to_rect(&ovl->cfg, hwc_dev->primary_region) != 0) {
        ovl->cfg.enabled = 0;
        return;
    }

    omap4_hwc_apply_transform(hwc_dev->primary_m, oc);

    /* combining transformations: F^a*R^b*F^i*R^j = F^(a+b)*R^(j+b*(-1)^i), because F*R = R^(-1)*F */
    oc->rotation += (oc->mirror ? -1 : 1) * hwc_dev->primary_rotation;
    oc->rotation &= 3;
}

static int omap4_hwc_can_scale(__u32 src_w, __u32 src_h, __u32 dst_w, __u32 dst_h, int is_2d,
                               struct dsscomp_display_info *dis, struct dsscomp_platform_info *limits,
                               __u32 pclk, void *handle)
{
    __u32 fclk = limits->fclk / 1000;
    __u32 min_src_w = DIV_ROUND_UP(src_w, is_2d ? limits->max_xdecim_2d : limits->max_xdecim_1d);
    __u32 min_src_h = DIV_ROUND_UP(src_h, is_2d ? limits->max_ydecim_2d : limits->max_ydecim_1d);

    /* ERRATAs */
    /* cannot render 1-width layers on DSI video mode panels - we just disallow all 1-width LCD layers */
    if (dis->channel != OMAP_DSS_CHANNEL_DIGIT && dst_w < limits->min_width)
        return 0;

    /* NOTE: no support for checking YUV422 layers that are tricky to scale */

    /* FIXME: limit vertical downscale well below theoretical limit as we saw display artifacts */
    if (dst_h < src_h / 4)
        return 0;

    /* max downscale */
    if (dst_h * limits->max_downscale < min_src_h)
        return 0;

    /* for manual panels pclk is 0, and there are no pclk based scaling limits */
    if (!pclk)
        return !(dst_w < src_w / limits->max_downscale / (is_2d ? limits->max_xdecim_2d : limits->max_xdecim_1d));

    /* :HACK: limit horizontal downscale well below theoretical limit as we saw display artifacts */
    if (dst_w * 4 < src_w)
        return 0;

    if (handle)
        if (get_rgb_bpp(handle) == 32 && src_w > 1280 && dst_w * 3 < src_w)
            return 0;

    /* max horizontal downscale is 4, or the fclk/pixclk */
    if (fclk > pclk * limits->max_downscale)
        fclk = pclk * limits->max_downscale;
    /* for small parts, we need to use integer fclk/pixclk */
    if (src_w < limits->integer_scale_ratio_limit)
        fclk = fclk / pclk * pclk;
    if ((__u32) dst_w * fclk < min_src_w * pclk)
        return 0;

    return 1;
}

static int omap4_hwc_can_scale_layer(omap4_hwc_device_t *hwc_dev, hwc_layer_t *layer, IMG_native_handle_t *handle)
{
    int src_w = WIDTH(layer->sourceCrop);
    int src_h = HEIGHT(layer->sourceCrop);
    int dst_w = WIDTH(layer->displayFrame);
    int dst_h = HEIGHT(layer->displayFrame);

    /* account for 90-degree rotation */
    if (layer->transform & HWC_TRANSFORM_ROT_90)
        swap(src_w, src_h);

    /* NOTE: layers should be able to be scaled externally since
       framebuffer is able to be scaled on selected external resolution */
    return omap4_hwc_can_scale(src_w, src_h, dst_w, dst_h, is_NV12(handle), &hwc_dev->fb_dis, &limits,
                               hwc_dev->fb_dis.timings.pixel_clock, handle);
}

static int omap4_hwc_is_valid_layer(omap4_hwc_device_t *hwc_dev,
                                    hwc_layer_t *layer,
                                    IMG_native_handle_t *handle)
{
    /* Skip layers are handled by SF */
    if ((layer->flags & HWC_SKIP_LAYER) || !handle)
        return 0;

    if (!omap4_hwc_is_valid_format(handle->iFormat))
        return 0;

    /* 1D buffers: no transform, must fit in TILER slot */
    if (!is_NV12(handle)) {
        if (layer->transform)
            return 0;
        if (mem1d(handle) > limits.tiler1d_slot_size)
            return 0;
    }

    return omap4_hwc_can_scale_layer(hwc_dev, layer, handle);
}

static void gather_layer_statistics(omap4_hwc_device_t *hwc_dev, struct counts *num, hwc_layer_list_t *list)
{
    unsigned int i;

    /* Figure out how many layers we can support via DSS */
    for (i = 0; list && i < list->numHwLayers; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        layer->compositionType = HWC_FRAMEBUFFER;

        if (omap4_hwc_is_valid_layer(hwc_dev, layer, handle)) {

            num->possible_overlay_layers++;

            /* NV12 layers can only be rendered on scaling overlays */
            if (scaled(layer) || is_NV12(handle) || hwc_dev->primary_transform)
                num->scaled_layers++;

            if (is_BGR(handle))
                num->BGR++;
            else if (is_RGB(handle))
                num->RGB++;
            else if (is_NV12(handle))
                num->NV12++;

            num->mem += mem1d(handle);
        }
    }
    hwc_dev->stats = *num;

    /* If FB is not same resolution as LCD don't use GFX pipe line*/
    if (hwc_dev->primary_transform) {
        num->max_hw_overlays = MAX_HW_OVERLAYS - NUM_NONSCALING_OVERLAYS;
        num->max_scaling_overlays = num->max_hw_overlays;
    } else
        num->max_hw_overlays = MAX_HW_OVERLAYS;
        num->max_scaling_overlays = num->max_hw_overlays - NUM_NONSCALING_OVERLAYS;
}

static int can_dss_render_all(omap4_hwc_device_t *hwc_dev, struct counts *num)
{
    return  !hwc_dev->force_sgx &&
            /* must have at least one layer if using composition bypass to get sync object */
            num->possible_overlay_layers &&
            num->possible_overlay_layers <= num->max_hw_overlays &&
            num->possible_overlay_layers == num->composited_layers &&
            num->scaled_layers <= num->max_scaling_overlays &&
            num->NV12 <= num->max_scaling_overlays &&
            /* fits into TILER slot */
            num->mem <= limits.tiler1d_slot_size &&
            (num->BGR == 0 || num->RGB == 0 || !hwc_dev->flags_rgb_order);
}

static inline int can_dss_render_layer(omap4_hwc_device_t *hwc_dev,
            hwc_layer_t *layer)
{
    IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;
    return omap4_hwc_is_valid_layer(hwc_dev, layer, handle) &&
           /* skip non-NV12 layers if also using SGX (if nv12_only flag is set) */
           (!hwc_dev->flags_nv12_only || (!hwc_dev->use_sgx || is_NV12(handle))) &&
           /* make sure RGB ordering is consistent (if rgb_order flag is set) */
           (!(hwc_dev->swap_rb ? is_RGB(handle) : is_BGR(handle)) || !hwc_dev->flags_rgb_order) ;
}

static inline int display_area(struct dss2_ovl_info *o)
{
    return o->cfg.win.w * o->cfg.win.h;
}

void debug_post2(omap4_hwc_device_t *hwc_dev, int nbufs)
{
    if (!debugpost2)
        return;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->comp_data.dsscomp_data;
    int i;
    for (i=0; i<nbufs; i++) {
        ALOGI("buf[%d] hndl %p", i, hwc_dev->buffers[i]);
    }
    for (i=0; i < dsscomp->num_ovls; i++) {
        ALOGI("ovl[%d] ba %d", i, dsscomp->ovls[i].ba);
    }
}

static int omap4_hwc_prepare(struct hwc_composer_device *dev, hwc_layer_list_t* list)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->comp_data.dsscomp_data;
    struct counts num = { .composited_layers = list ? list->numHwLayers : 0 };
    unsigned int i, ix;

    pthread_mutex_lock(&hwc_dev->lock);
    memset(dsscomp, 0x0, sizeof(*dsscomp));
    dsscomp->sync_id = sync_id++;

    gather_layer_statistics(hwc_dev, &num, list);

    /* phase 3 logic */
    if (can_dss_render_all(hwc_dev, &num)) {
        /* All layers can be handled by the DSS -- don't use SGX for composition */
        hwc_dev->use_sgx = 0;
        hwc_dev->swap_rb = num.BGR != 0;
    } else {
        /* Use SGX for composition plus first 3 layers that are DSS renderable */
        hwc_dev->use_sgx = 1;
        hwc_dev->swap_rb = is_BGR_format(hwc_dev->fb_dev->base.format);
    }

    /* setup pipes */
    int z = 0;
    int fb_z = -1;
    int scaled_gfx = 0;
    int needs_fb = hwc_dev->use_sgx;

    /* If a framebuffer is needed, begin using VID1 for DSS overlay layers,
     * we need GFX for FB
     */
    dsscomp->num_ovls = needs_fb ? 1 /*VID1*/ : 0 /*GFX*/;

    /* set up if DSS layers */
    unsigned int mem_used = 0;
    for (i = 0; list && i < list->numHwLayers ; i++) {
        hwc_layer_t *layer = &list->hwLayers[i];
        IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;

        if (dsscomp->num_ovls < num.max_hw_overlays &&
            can_dss_render_layer(hwc_dev, layer) &&
            (!hwc_dev->force_sgx ||
             /* render protected and dockable layers via DSS */
             is_protected(layer) ||
             is_upscaled_NV12(hwc_dev, layer)) &&
            /* can't have a transparent overlay in the middle of the framebuffer stack */
            !(is_BLENDED(layer) && fb_z >= 0)) {

            /* render via DSS overlay */
            mem_used += mem1d(handle);
            layer->compositionType = HWC_OVERLAY;
            /*
             * This hint will not be used in vanilla ICS, but maybe in
             * JellyBean, it is useful to distinguish between blts and true
             * overlays
             */
            layer->hints |= HWC_HINT_TRIPLE_BUFFER;

            /* clear FB above all opaque layers if rendering via SGX */
            if (hwc_dev->use_sgx && !is_BLENDED(layer))
                layer->hints |= HWC_HINT_CLEAR_FB;

            hwc_dev->buffers[dsscomp->num_ovls] = layer->handle;
            //ALOGI("dss buffers[%d] = %p", dsscomp->num_ovls, hwc_dev->buffers[dsscomp->num_ovls]);

            omap4_hwc_setup_layer(hwc_dev,
                                  &dsscomp->ovls[dsscomp->num_ovls],
                                  layer,
                                  z,
                                  handle->iFormat,
                                  handle->iWidth,
                                  handle->iHeight);

            dsscomp->ovls[dsscomp->num_ovls].cfg.ix = dsscomp->num_ovls + hwc_dev->primary_transform;
            dsscomp->ovls[dsscomp->num_ovls].addressing = OMAP_DSS_BUFADDR_LAYER_IX;
            dsscomp->ovls[dsscomp->num_ovls].ba = dsscomp->num_ovls;

            /* ensure GFX layer is never scaled */
            if ((dsscomp->num_ovls == 0) && (!hwc_dev->primary_transform)) {
                scaled_gfx = scaled(layer) || is_NV12(handle);
            } else if (scaled_gfx && !scaled(layer) && !is_NV12(handle)) {
                /* swap GFX layer with this one */
                dsscomp->ovls[dsscomp->num_ovls].cfg.ix = 0;
                dsscomp->ovls[0].cfg.ix = dsscomp->num_ovls;
                scaled_gfx = 0;
            }

            dsscomp->num_ovls++;
            z++;
        } else if (hwc_dev->use_sgx) {
            if (fb_z < 0) {
                /* NOTE: we are not handling transparent cutout for now */
                fb_z = z;
                z++;
            } else {
                /* move fb z-order up (by lowering dss layers) */
                while (fb_z < z - 1)
                    dsscomp->ovls[1 + fb_z++].cfg.zorder--;
            }
        }
    }

    /* if scaling GFX (e.g. only 1 scaled surface) use a VID pipe */
    if (scaled_gfx)
        dsscomp->ovls[0].cfg.ix = dsscomp->num_ovls;

    /* If the SGX is not used and there is blit data we need a framebuffer and
     * a DSS pipe well configured for it
     */
    if (needs_fb) {
        /* assign a z-layer for fb */
        if (fb_z < 0) {
            if (!hwc_dev->blt_policy != BLTPOLICY_DISABLED && num.composited_layers)
                ALOGE("**** should have assigned z-layer for fb");
            fb_z = z++;
        }
        hwc_dev->buffers[0] = NULL;
        omap4_hwc_setup_layer_base(&dsscomp->ovls[0].cfg, fb_z,
                                   hwc_dev->fb_dev->base.format,
                                   1,   /* FB is always premultiplied */
                                   hwc_dev->fb_dev->base.width,
                                   hwc_dev->fb_dev->base.height);
        dsscomp->ovls[0].cfg.pre_mult_alpha = 1;
        dsscomp->ovls[0].addressing = OMAP_DSS_BUFADDR_LAYER_IX;
        dsscomp->ovls[0].ba = 0;
        dsscomp->ovls[0].cfg.ix = hwc_dev->primary_transform;
    }

    hwc_dev->post2_layers = dsscomp->num_ovls;

    /* Apply transform for primary display */
    if (hwc_dev->primary_transform)
        for (i = 0; i < dsscomp->num_ovls; i++) {
            if(dsscomp->ovls[i].cfg.mgr_ix == 0)
                omap4_hwc_adjust_primary_display_layer(hwc_dev, &dsscomp->ovls[i]);
        }

    if (z != dsscomp->num_ovls || dsscomp->num_ovls > MAX_HW_OVERLAYS)
        ALOGE("**** used %d z-layers for %d overlays\n", z, dsscomp->num_ovls);

    /* verify all z-orders and overlay indices are distinct */
    for (i = z = ix = 0; i < dsscomp->num_ovls; i++) {
        struct dss2_ovl_cfg *c = &dsscomp->ovls[i].cfg;

        if (z & (1 << c->zorder))
            ALOGE("**** used z-order #%d multiple times", c->zorder);
        if (ix & (1 << c->ix))
            ALOGE("**** used ovl index #%d multiple times", c->ix);
        z |= 1 << c->zorder;
        ix |= 1 << c->ix;
    }
    dsscomp->mode = DSSCOMP_SETUP_DISPLAY;
    dsscomp->mgrs[0].ix = 0;
    dsscomp->mgrs[0].alpha_blending = 1;
    dsscomp->mgrs[0].swap_rb = hwc_dev->swap_rb;
    dsscomp->num_mgrs = 1;

    if (debug) {
        ALOGD("prepare (%d) - %s (comp=%d, poss=%d/%d scaled, RGB=%d,BGR=%d,NV12=%d) \n",
             dsscomp->sync_id,
             hwc_dev->use_sgx ? "SGX+OVL" : "all-OVL",
             num.composited_layers,
             num.possible_overlay_layers, num.scaled_layers,
             num.RGB, num.BGR, num.NV12);
    }

    pthread_mutex_unlock(&hwc_dev->lock);
    return 0;
}

static void omap4_hwc_reset_screen(omap4_hwc_device_t *hwc_dev)
{
    static int first_set = 1;
    int ret;

    if (first_set) {
        first_set = 0;
        struct dsscomp_setup_dispc_data d = {
            .num_mgrs = 1,
        };
        /* remove bootloader image from the screen as blank/unblank does not change the composition */
        ret = ioctl(hwc_dev->dsscomp_fd, DSSCIOC_SETUP_DISPC, &d);
        if (ret)
            ALOGW("failed to remove bootloader image");

        /* blank and unblank fd to make sure display is properly programmed on boot.
         * This is needed because the bootloader can not be trusted.
         */
        ret = ioctl(hwc_dev->fb_fd, FBIOBLANK, FB_BLANK_POWERDOWN);
        if (ret)
            ALOGW("failed to blank display");

        ret = ioctl(hwc_dev->fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);
        if (ret)
            ALOGW("failed to blank display");
    }
}

static int omap4_hwc_set(struct hwc_composer_device *dev, hwc_display_t dpy,
               hwc_surface_t sur, hwc_layer_list_t* list)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->comp_data.dsscomp_data;
    int err = 0;

    pthread_mutex_lock(&hwc_dev->lock);

    omap4_hwc_reset_screen(hwc_dev);

    if (debug)
        dump_set_info(hwc_dev, list);

    if (dpy && sur) {
        // list can be NULL which means hwc is temporarily disabled.
        // however, if dpy and sur are null it means we're turning the
        // screen off. no shall not call eglSwapBuffers() in that case.

        if (hwc_dev->use_sgx) {
            if (!eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur)) {
                ALOGE("eglSwapBuffers error");
                err = HWC_EGL_ERROR;
                goto err_out;
            }
        }

        //dump_dsscomp(dsscomp);

        // signal the event thread that a post has happened
        write(hwc_dev->pipe_fds[1], "s", 1);
        if (hwc_dev->force_sgx > 0)
            hwc_dev->force_sgx--;

        debug_post2(hwc_dev, hwc_dev->post2_layers);
        err = hwc_dev->fb_dev->Post2((framebuffer_device_t *)hwc_dev->fb_dev,
                                 hwc_dev->buffers,
                                 hwc_dev->post2_layers,
                                 dsscomp, sizeof(hwc_dev->comp_data));
        showfps();
    }
    hwc_dev->last_int_ovls = hwc_dev->post2_layers;
    if (err)
        ALOGE("Post2 error");

err_out:
    pthread_mutex_unlock(&hwc_dev->lock);

    return err;
}

static void omap4_hwc_dump(struct hwc_composer_device *dev, char *buff, int buff_len)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *)dev;
    struct dsscomp_setup_dispc_data *dsscomp = &hwc_dev->comp_data.dsscomp_data;
    struct dump_buf log = {
        .buf = buff,
        .buf_len = buff_len,
    };
    int i;

    dump_printf(&log, "omap4_hwc %d:\n", dsscomp->num_ovls);
    dump_printf(&log, "  idle timeout: %dms\n", hwc_dev->idle);

    for (i = 0; i < dsscomp->num_ovls; i++) {
        struct dss2_ovl_cfg *cfg = &dsscomp->ovls[i].cfg;

        dump_printf(&log, "  layer %d:\n", i);
        dump_printf(&log, "     enabled:%s buff:%p %dx%d stride:%d\n",
                          cfg->enabled ? "true" : "false", hwc_dev->buffers[i],
                          cfg->width, cfg->height, cfg->stride);
        dump_printf(&log, "     src:(%d,%d) %dx%d dst:(%d,%d) %dx%d ix:%d zorder:%d\n",
                          cfg->crop.x, cfg->crop.y, cfg->crop.w, cfg->crop.h,
                          cfg->win.x, cfg->win.y, cfg->win.w, cfg->win.h,
                          cfg->ix, cfg->zorder);
    }

    dump_printf(&log, "\n");
}

static int omap4_hwc_device_close(hw_device_t* device)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *) device;;

    if (hwc_dev) {
        if (hwc_dev->dsscomp_fd >= 0)
            close(hwc_dev->dsscomp_fd);
        if (hwc_dev->fb_fd >= 0)
            close(hwc_dev->fb_fd);

        /* pthread will get killed when parent process exits */
        pthread_mutex_destroy(&hwc_dev->lock);
        free(hwc_dev);
    }

    return 0;
}

static int omap4_hwc_open_fb_hal(IMG_framebuffer_device_public_t **fb_dev)
{
    const struct hw_module_t *psModule;
    IMG_gralloc_module_public_t *psGrallocModule;
    int err;

    err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &psModule);
    psGrallocModule = (IMG_gralloc_module_public_t *) psModule;

    if(err)
        goto err_out;

    if (strcmp(psGrallocModule->base.common.author, "Imagination Technologies")) {
        err = -EINVAL;
        goto err_out;
    }

    *fb_dev = psGrallocModule->psFrameBufferDevice;

    return 0;

err_out:
    ALOGE("Composer HAL failed to load compatible Graphics HAL");
    return err;
}

static void set_primary_display_transform_matrix(omap4_hwc_device_t *hwc_dev)
{
    /* create primary display translation matrix */
    hwc_dev->fb_dis.ix = 0;/*Default display*/

    int ret = ioctl(hwc_dev->dsscomp_fd, DSSCIOC_QUERY_DISPLAY, &hwc_dev->fb_dis);
    if (ret)
        ALOGE("failed to get display info (%d): %m", errno);

    int lcd_w = hwc_dev->fb_dis.timings.x_res;
    int lcd_h = hwc_dev->fb_dis.timings.y_res;
    int orig_w = hwc_dev->fb_dev->base.width;
    int orig_h = hwc_dev->fb_dev->base.height;
    hwc_rect_t region = {.left = 0, .top = 0, .right = orig_w, .bottom = orig_h};
    hwc_dev->primary_region = region;
    hwc_dev->primary_rotation = ((lcd_w > lcd_h) ^ (orig_w > orig_h)) ? 1 : 0;
    hwc_dev->primary_transform = ((lcd_w != orig_w)||(lcd_h != orig_h)) ? 1 : 0;

    ALOGI("transforming FB (%dx%d) => (%dx%d) rot%d", orig_w, orig_h, lcd_w, lcd_h, hwc_dev->primary_rotation);

    /* reorientation matrix is:
       m = (center-from-target-center) * (scale-to-target) * (mirror) * (rotate) * (center-to-original-center) */

    memcpy(hwc_dev->primary_m, m_unit, sizeof(m_unit));
    m_translate(hwc_dev->primary_m, -(orig_w >> 1), -(orig_h >> 1));
    m_rotate(hwc_dev->primary_m, hwc_dev->primary_rotation);
    if (hwc_dev->primary_rotation & 1)
         swap(orig_w, orig_h);
    m_scale(hwc_dev->primary_m, orig_w, lcd_w, orig_h, lcd_h);
    m_translate(hwc_dev->primary_m, lcd_w >> 1, lcd_h >> 1);
}

static inline void handle_uevents(omap4_hwc_device_t *hwc_dev, const char *buff, int len)
{
    int state = 0;
    uint64_t timestamp = 0;
    const char *s = buff;
    int vsync = !strcmp(s, "change@/devices/platform/omapfb") || !strcmp(s, "change@/devices/virtual/switch/omapfb-vsync");
    if (!vsync)
       return;

    s += strlen(s) + 1;
    while(*s) {
        if (!strncmp(s, "VSYNC=", strlen("VSYNC=")))
            timestamp = strtoull(s + strlen("VSYNC="), NULL, 0);
        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    if (hwc_dev->procs && hwc_dev->procs->vsync) 
        hwc_dev->procs->vsync(hwc_dev->procs, 0, timestamp);
}
	
static void *omap4_hwc_hdmi_thread(void *data)
{
    omap4_hwc_device_t *hwc_dev = data;
    static char uevent_desc[4096];
    struct pollfd fds[2];
    int invalidate = 0;
    int timeout;
    int err;

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    uevent_init();

    fds[0].fd = uevent_get_fd();
    fds[0].events = POLLIN;
    fds[1].fd = hwc_dev->pipe_fds[0];
    fds[1].events = POLLIN;

    timeout = hwc_dev->idle ? hwc_dev->idle : -1;

    memset(uevent_desc, 0, sizeof(uevent_desc));

    do {
        err = poll(fds, hwc_dev->idle ? 2 : 1, timeout);

        if (err == 0) {
            if (hwc_dev->idle) {
                if (hwc_dev->procs && hwc_dev->procs->invalidate) {
                    pthread_mutex_lock(&hwc_dev->lock);
                    invalidate = hwc_dev->last_int_ovls > 1 && !hwc_dev->force_sgx;
                    if (invalidate) {
                        hwc_dev->force_sgx = 2;
                    }
                    pthread_mutex_unlock(&hwc_dev->lock);

                    if (invalidate) {
                        hwc_dev->procs->invalidate(hwc_dev->procs);
                        timeout = -1;
                    }
                }

                continue;
            }
        }

        if (err == -1) {
            if (errno != EINTR)
                ALOGE("event error: %m");
            continue;
        }

        if (hwc_dev->idle && fds[1].revents & POLLIN) {
            char c;
            read(hwc_dev->pipe_fds[0], &c, 1);
            if (!hwc_dev->force_sgx)
                timeout = hwc_dev->idle ? hwc_dev->idle : -1;
        }

        if (fds[0].revents & POLLIN) {
            /* keep last 2 zeroes to ensure double 0 termination */
            int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
            handle_uevents(hwc_dev, uevent_desc, len);
        }
    } while (1);

    return NULL;
}

static void omap4_hwc_registerProcs(struct hwc_composer_device* dev,
                                    hwc_procs_t const* procs)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *) dev;

    hwc_dev->procs = (typeof(hwc_dev->procs)) procs;
}

static int omap4_hwc_query(struct hwc_composer_device* dev,
        int what, int* value)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *) dev;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // we don't support the background layer yet
        value[0] = 0;
        break;
    case HWC_VSYNC_PERIOD:
        // vsync period in nanosecond
        value[0] = 1000000000.0 / hwc_dev->fb_dev->base.fps;
        break;
    default:
        // unsupported query
        return -EINVAL;
    }
    return 0;
}

static int omap4_hwc_event_control(struct hwc_composer_device* dev,
        int event, int enabled)
{
    omap4_hwc_device_t *hwc_dev = (omap4_hwc_device_t *) dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
    {
        int val = !!enabled;
        int err;

        err = ioctl(hwc_dev->fb_fd, OMAPFB_ENABLEVSYNC, &val);
        if (err < 0)
            return -errno;

        return 0;
    }
    default:
        return -EINVAL;
    }
}

struct hwc_methods omap4_hwc_methods = {
    .eventControl = &omap4_hwc_event_control,
};

static int omap4_hwc_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    omap4_hwc_module_t *hwc_mod = (omap4_hwc_module_t *)module;
    omap4_hwc_device_t *hwc_dev;
    int err = 0;

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        return -EINVAL;
    }

    if (!hwc_mod->fb_dev) {
        err = omap4_hwc_open_fb_hal(&hwc_mod->fb_dev);
        if (err)
            return err;

        if (!hwc_mod->fb_dev) {
            ALOGE("Framebuffer HAL not opened before HWC");
            return -EFAULT;
        }
        hwc_mod->fb_dev->bBypassPost = 1;
    }

    hwc_dev = (omap4_hwc_device_t *)malloc(sizeof(*hwc_dev));
    if (hwc_dev == NULL)
        return -ENOMEM;

    memset(hwc_dev, 0, sizeof(*hwc_dev));
    char value[PROPERTY_VALUE_MAX];

    hwc_dev->base.common.tag = HARDWARE_DEVICE_TAG;
    hwc_dev->base.common.version = HWC_DEVICE_API_VERSION_0_2;
    hwc_dev->base.common.module = (hw_module_t *)module;
    hwc_dev->base.common.close = omap4_hwc_device_close;
    hwc_dev->base.prepare = omap4_hwc_prepare;
    hwc_dev->base.set = omap4_hwc_set;
    hwc_dev->base.dump = omap4_hwc_dump;
    hwc_dev->base.registerProcs = omap4_hwc_registerProcs;
    hwc_dev->base.query = omap4_hwc_query;
    hwc_dev->base.methods = &omap4_hwc_methods;
    hwc_dev->fb_dev = hwc_mod->fb_dev;
    *device = &hwc_dev->base.common;

    hwc_dev->dsscomp_fd = open("/dev/dsscomp", O_RDWR);
    if (hwc_dev->dsscomp_fd < 0) {
        ALOGE("failed to open dsscomp (%d)", errno);
        err = -errno;
        goto done;
    }

    int ret = ioctl(hwc_dev->dsscomp_fd, DSSCIOC_QUERY_PLATFORM, &limits);
    if (ret) {
        ALOGE("failed to get platform limits (%d): %m", errno);
        err = -errno;
        goto done;
    }

    hwc_dev->fb_fd = open("/dev/graphics/fb0", O_RDWR);
    if (hwc_dev->fb_fd < 0) {
        ALOGE("failed to open fb (%d)", errno);
        err = -errno;
        goto done;
    }

    struct fb_fix_screeninfo fix;
    if (ioctl(hwc_dev->fb_fd, FBIOGET_FSCREENINFO, &fix)) {
        ALOGE("failed to get fb info (%d)", errno);
        err = -errno;
        goto done;
    }

    /* Allocate the maximum buffers that we can receive from HWC */
    hwc_dev->buffers = malloc(sizeof(buffer_handle_t) * MAX_HWC_LAYERS);
    if (!hwc_dev->buffers) {
        err = -ENOMEM;
        goto done;
    }

    ret = ioctl(hwc_dev->dsscomp_fd, DSSCIOC_QUERY_DISPLAY, &hwc_dev->fb_dis);
    if (ret) {
        ALOGE("failed to get display info (%d): %m", errno);
        err = -errno;
        goto done;
    }

    set_primary_display_transform_matrix(hwc_dev);

    if (pipe(hwc_dev->pipe_fds) == -1) {
            ALOGE("failed to event pipe (%d): %m", errno);
            err = -errno;
            goto done;
    }

    if (pthread_mutex_init(&hwc_dev->lock, NULL)) {
        ALOGE("failed to create mutex (%d): %m", errno);
        err = -errno;
        goto done;
    }
    if (pthread_create(&hwc_dev->hdmi_thread, NULL, omap4_hwc_hdmi_thread, hwc_dev))
    {
        ALOGE("failed to create HDMI listening thread (%d): %m", errno);
        err = -errno;
        goto done;
    }

    /* get debug properties */
    /* see if hwc is enabled at all */
    property_get("debug.hwc.rgb_order", value, "1");
    hwc_dev->flags_rgb_order = atoi(value);
    property_get("debug.hwc.nv12_only", value, "0");
    hwc_dev->flags_nv12_only = atoi(value);
    property_get("debug.hwc.idle", value, "250");
    hwc_dev->idle = atoi(value);

    ALOGI("omap4_hwc_device_open(rgb_order=%d nv12_only=%d)",
        hwc_dev->flags_rgb_order, hwc_dev->flags_nv12_only);

    ALOGI("omap4430/4460 do not have gc-core , blits disabled");
    hwc_dev->blt_policy = BLTPOLICY_DISABLED;

    property_get("persist.hwc.upscaled_nv12_limit", value, "2.");
    sscanf(value, "%f", &hwc_dev->upscaled_nv12_limit);
    if (hwc_dev->upscaled_nv12_limit < 0. || hwc_dev->upscaled_nv12_limit > 2048.) {
        ALOGW("Invalid upscaled_nv12_limit (%s), setting to 2.", value);
        hwc_dev->upscaled_nv12_limit = 2.;
    }

done:
    if (err && hwc_dev) {
        if (hwc_dev->dsscomp_fd >= 0)
            close(hwc_dev->dsscomp_fd);
        if (hwc_dev->fb_fd >= 0)
            close(hwc_dev->fb_fd);
        pthread_mutex_destroy(&hwc_dev->lock);
        free(hwc_dev->buffers);
        free(hwc_dev);
    }

    return err;
}

static struct hw_module_methods_t omap4_hwc_module_methods = {
    .open = omap4_hwc_device_open,
};

omap4_hwc_module_t HAL_MODULE_INFO_SYM = {
    .base = {
        .common = {
            .tag =                  HARDWARE_MODULE_TAG,
            .module_api_version =   HWC_MODULE_API_VERSION_0_1,
            .hal_api_version =      HARDWARE_HAL_API_VERSION,
            .id =                   HWC_HARDWARE_MODULE_ID,
            .name =                 "OMAP 44xx Hardware Composer HAL",
            .author =               "Texas Instruments",
            .methods =              &omap4_hwc_module_methods,
        },
    },
};
