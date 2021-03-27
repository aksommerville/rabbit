#define RB_DEMO_USE_CB_CLICK 1

#include "rb_demo.h"
#include "rabbit/rb_font.h"

static struct rb_image *font=0;
static int timer=0;
static uint8_t xform=RB_XFORM_NONE;
static struct rb_image *message=0;

static void demo_xform_quit() {
  rb_image_del(font);
  font=0;
  rb_image_del(message);
  message=0;
}

static int demo_xform_init() {
  if (!(font=rb_font_generate_minimal())) {
    fprintf(stderr,"failed to generate font\n");
    return -1;
  }
  message=rb_font_print(
    font,RB_FONTCONTENT_G0,RB_FONT_FLAG_MARGINL|RB_FONT_FLAG_MARGINT,0,
    "Click to view all xforms.",-1
  );
  return 0;
}

static uint32_t blend_dark_blue(uint32_t dst,uint32_t src,void *userdata) {
  return 0xff000080;
}

static uint32_t blend_white(uint32_t dst,uint32_t src,void *userdata) {
  return 0xffffffff;
}

static void advance_setup() {
  if (++xform>=8) xform=0;
  if (message) rb_image_del(message);
  message=rb_font_printf(
    font,RB_FONTCONTENT_G0,RB_FONT_FLAG_MARGINL|RB_FONT_FLAG_MARGINT,0,
    "xform 0x%02x%s%s%s",
    xform,
    (xform&RB_XFORM_SWAP)?" SWAP":"",
    (xform&RB_XFORM_XREV)?" XREV":"",
    (xform&RB_XFORM_YREV)?" YREV":""
  );
}

static int demo_xform_cb_click(int x,int y,int value) {
  if (value) {
    advance_setup();
  }
  return 0;
}

static int demo_xform_update() {

  if (--timer<=0) {
    timer=30;
    //advance_setup();
  }

  memset(rb_demo_fb->pixels,0x80,RB_FB_SIZE_BYTES);
  
  // Draw the font image in the middle of the framebuffer with no xform, for reference.
  int dstx=(RB_FB_W>>1)-(font->w>>1);
  int dsty=(RB_FB_H>>1)-(font->h>>1);
  if (rb_image_blit_safe(
    rb_demo_fb,dstx,dsty,
    font,0,0,
    font->w,font->h,
    xform,blend_dark_blue,0
  )<0) return -1;
  
  // Now draw it 8 more times, halfway off each of the edges.
  if (1) {
    int py=-1; for (;py<=1;py++) {
      int px=-1; for (;px<=1;px++) {
        if (!py&&!px) continue;
        if (rb_image_blit_safe(
          rb_demo_fb,dstx+px*(RB_FB_W>>1),dsty+py*(RB_FB_H>>1),
          font,0,0,
          font->w,font->h,
          xform,0,0
        )<0) return -1;
      }
    }
  }
  
  if (message) {
    rb_image_blit_safe(
      rb_demo_fb,(RB_FB_W>>1)-(message->w>>1),(RB_FB_H>>2),
      message,0,0,
      message->w,message->h,
      0,blend_white,0
    );
  }
  
  return 1;
}

RB_DEMO(xform)
