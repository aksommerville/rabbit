#include "rabbit/rb_internal.h"
#include "rabbit/rb_video.h"
#include "rabbit/rb_image.h"
#include <bcm_host.h>

// Screen size sanity limit.
#define RB_BCM_SIZE_LIMIT 4096

/* Instance.
 */

struct rb_video_bcm {
  struct rb_video hdr;

  DISPMANX_DISPLAY_HANDLE_T vcdisplay;
  DISPMANX_RESOURCE_HANDLE_T vcresource;
  DISPMANX_ELEMENT_HANDLE_T vcelement;
  DISPMANX_UPDATE_HANDLE_T vcupdate;
  
  int screenw,screenh;
  void *fb;
  int vsync_seq;
};

#define VIDEO ((struct rb_video_bcm*)video)

/* Cleanup.
 */

static void _rb_bcm_del(struct rb_video *video) {
  if (VIDEO->fb) free(VIDEO->fb);
  bcm_host_deinit();
}

/* Init.
 */

static void rb_bcm_cb_vsync(DISPMANX_UPDATE_HANDLE_T update,void *arg) {
  struct rb_video *video=arg;
  VIDEO->vsync_seq++;
}

static int _rb_bcm_init(struct rb_video *video) {

  bcm_host_init();

  graphics_get_display_size(0,&VIDEO->screenw,&VIDEO->screenh);
  if (
    (VIDEO->screenw<1)||(VIDEO->screenw>RB_BCM_SIZE_LIMIT)||
    (VIDEO->screenh<1)||(VIDEO->screenh>RB_BCM_SIZE_LIMIT)
  ) return -1;

  if (!(VIDEO->vcdisplay=vc_dispmanx_display_open(0))) return -1;
  if (!(VIDEO->vcupdate=vc_dispmanx_update_start(0))) return -1;

  VC_RECT_T srcr={0,0,RB_FB_W<<16,RB_FB_H<<16};
  VC_RECT_T dstr={0,0,VIDEO->screenw,VIDEO->screenh};

  if (!(VIDEO->fb=malloc(4*RB_FB_W*RB_FB_H))) return -1;
  if (!(VIDEO->vcresource=vc_dispmanx_resource_create(
    VC_IMAGE_XRGB8888,RB_FB_W,RB_FB_H,VIDEO->fb
  ))) return -1;

  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xffffffff};
  if (!(VIDEO->vcelement=vc_dispmanx_element_add(
    VIDEO->vcupdate,VIDEO->vcdisplay,1,&dstr,VIDEO->vcresource,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  ))) return -1;

  if (vc_dispmanx_update_submit_sync(VIDEO->vcupdate)<0) return -1;

  if (vc_dispmanx_vsync_callback(VIDEO->vcdisplay,rb_bcm_cb_vsync,video)<0) return -1;

  return 0;
}

/* Swap buffers.
 */

static int _rb_bcm_swap(struct rb_video *video,struct rb_image *fb) {
  int wait_vsync_seq=VIDEO->vsync_seq+1;

  // getting a bunch of red in the output... do we need to clear the MSBs?
  {
    uint32_t *v=fb->pixels;
    int i=fb->w*fb->h;
    for (;i-->0;v++) (*v)&=0x00ffffff;
  }
  
  // This is enough to replace the screen content and make it live. Cool!
  VC_RECT_T fbr={0,0,RB_FB_W,RB_FB_H};
  vc_dispmanx_resource_write_data(VIDEO->vcresource,VC_IMAGE_XRGB8888,RB_FB_W<<2,fb->pixels,&fbr);

  /* Block manually until vsync. */
  int panic=100;
  while (1) {
    if (VIDEO->vsync_seq>=wait_vsync_seq) break;
    if (!panic--) {
      fprintf(stderr,"PANIC waiting for manual vsync!\n");
      return -1;
    }
    usleep(1000);
  }

  //TODO I'm sure we could get smarter about vsync and avoid the tearing that i see today

  return 0;
}

/* Type.
 */

static struct rb_video_bcm _rb_bcm_singleton={0};

const struct rb_video_type rb_video_type_bcm={
  .name="bcm",
  .desc="Broadcom video for Raspberry Pi",
  .objlen=sizeof(struct rb_video_bcm),
  .singleton=&_rb_bcm_singleton,
  .del=_rb_bcm_del,
  .init=_rb_bcm_init,
  .swap=_rb_bcm_swap,
};
