/* rb_drmgx_glue.c
 * Defines a rabbit-friendly video driver. The rest of this drmgx unit is not rabbit savvy.
 */
 
#include <stdint.h>
#include <stdio.h>
#include "drmgx.h"
#include "rabbit/rb_image.h"
#include "rabbit/rb_video.h"

struct rb_video_drmgx {
  struct rb_video hdr;
  struct drmgx *drmgx;
};

#define VIDEO ((struct rb_video_drmgx*)video)

static void _drmgx_del(struct rb_video *video) {
  drmgx_del(VIDEO->drmgx);
}

static int _drmgx_init(struct rb_video *video) {
  if (!(VIDEO->drmgx=drmgx_new(
    video->delegate.device,
    60, // rate
    video->delegate.winw,
    video->delegate.winh,
    0, // filter
    0 // glsl version
  ))) return -1;
  return 0;
}

static int _drmgx_swap(struct rb_video *video,struct rb_image *fb) {
  return drmgx_swap(VIDEO->drmgx,fb->pixels,fb->w,fb->h,DRMGX_FMT_RGBX);
}

const struct rb_video_type rb_video_type_drmgx={
  .name="drmgx",
  .desc="DRM with OpenGL ES, for Linux without a window manager.",
  .objlen=sizeof(struct rb_video_drmgx),
  .del=_drmgx_del,
  .init=_drmgx_init,
  .del=_drmgx_del,
  .swap=_drmgx_swap,
};
