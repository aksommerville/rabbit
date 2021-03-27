#include "rb_demo.h"
#include "rabbit/rb_inmgr.h"
#include "rabbit/rb_archive.h"

static struct rb_inmgr *inmgr=0;
static int dirty=0;
static struct rb_image *srcimage=0;

static int demo_inmgr_cb_res(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  if (restype!=RB_RES_TYPE_imag) return 0;
  if (resid!=2) return 0;
  if (!(srcimage=rb_image_new_decode(src,srcc))) return -1;
  if (srcimage->w!=256) return -1;
  if (srcimage->h!=256) return -1;
  return 1;
}

static int demo_inmgr_event(struct rb_inmgr *inmgr,const struct rb_input_event *event) {
  fprintf(stderr,
    "%s (%p:%d.%d=%d) => %d.%d=%d [%04x]\n",
    __func__,
    event->source,event->devid,event->devbtnid,event->devvalue,
    event->plrid,event->btnid,event->value,event->state
  );
  dirty=1;
  return 0;
}

static int demo_inmgr_init() {

  if (rb_archive_read("out/data",demo_inmgr_cb_res,0)<=0) return -1;

  struct rb_inmgr_delegate delegate={
    .cb_event=demo_inmgr_event,
  };
  if (!(inmgr=rb_inmgr_new(&delegate))) return -1;
  if (rb_inmgr_connect_all(inmgr)<0) return -1;
  if (rb_inmgr_set_player_count(inmgr,7)<0) return -1;
  dirty=1;
  return 0;
}

static void demo_inmgr_quit() {
  rb_inmgr_del(inmgr);
  inmgr=0;
}

static void demo_inmgr_fill_bg() {
  const int srcx=0;
  const int srcy=64;
  int dsty=0; for (;dsty<RB_FB_H;dsty+=16) {
    int dstx=0; for (;dstx<RB_FB_W;dstx+=16) {
      rb_image_blit_unchecked(
        rb_demo_fb,dstx,dsty,
        srcimage,srcx,srcy,
        16,16,
        0,0,0
      );
    }
  }
}

static void demo_inmgr_draw_joystick(int dstx,int dsty,uint16_t state) {

  #define CP(dx,dy,srcx,srcy,w,h,xform) \
    rb_image_blit_unchecked( \
      rb_demo_fb,dstx+dx,dsty+dy, \
      srcimage,srcx,srcy, \
      w,h, \
      xform,0,0 \
    );

  if (state&RB_BTNID_CD) CP(0,0,16,64,64,32,0)
  else CP(0,0,16,96,64,32,0)
  
  if (state&RB_BTNID_UP) CP(12,0,96,80,16,16,0)
  else CP(12,0,80,80,16,16,0)
  
  if (state&RB_BTNID_DOWN) CP(12,16,96,80,16,16,RB_XFORM_YREV)
  else CP(12,16,80,80,16,16,RB_XFORM_YREV)
  
  if (state&RB_BTNID_LEFT) CP(4,8,96,80,16,16,RB_XFORM_SWAP)
  else CP(4,8,80,80,16,16,RB_XFORM_SWAP)
  
  if (state&RB_BTNID_RIGHT) CP(20,8,96,80,16,16,RB_XFORM_SWAP|RB_XFORM_YREV)
  else CP(20,8,80,80,16,16,RB_XFORM_SWAP|RB_XFORM_YREV)
  
  if (state&RB_BTNID_A) CP(38,16,96,96,16,16,0)
  else CP(38,16,80,96,16,16,0)
  
  if (state&RB_BTNID_B) CP(32,10,96,96,16,16,0)
  else CP(32,10,80,96,16,16,0)
  
  if (state&RB_BTNID_C) CP(44,10,96,96,16,16,0)
  else CP(44,10,80,96,16,16,0)
  
  if (state&RB_BTNID_D) CP(38,4,96,96,16,16,0)
  else CP(38,4,80,96,16,16,0)
  
  if (state&RB_BTNID_L) CP(4,-8,96,64,16,16,0)
  else CP(4,-8,80,64,16,16,0)
  
  if (state&RB_BTNID_R) CP(44,-8,96,64,16,16,RB_XFORM_XREV)
  else CP(44,-8,80,64,16,16,RB_XFORM_XREV)
  
  if (state&RB_BTNID_START) CP(30,0,96,112,16,16,0)
  else CP(30,0,80,112,16,16,0)
  
  if (state&RB_BTNID_SELECT) CP(21,0,96,112,16,16,0)
  else CP(21,0,80,112,16,16,0)
  
  #undef CP
}

static void demo_inmgr_draw() {
  demo_inmgr_fill_bg();
  int plrid=0; for (;plrid<8;plrid++) {
    int dstx=(plrid&3)*64;
    int dsty=((plrid>>2)*72)+20;
    demo_inmgr_draw_joystick(dstx,dsty,rb_inmgr_get_state(inmgr,plrid));
  }
}

static int demo_inmgr_update() {
  if (rb_inmgr_update(inmgr)<0) return -1;
  if (dirty) {
    dirty=0;
    demo_inmgr_draw();
  }
  return 1;
}

RB_DEMO(inmgr)
