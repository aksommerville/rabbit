#include "rb_demo.h"
#include <math.h>

/* At 16x16, greyskull can do 3000 alpha sprites without missing a beat.
 * Disable alpha and it reaches 12000.
 * I reckon 200-300 tiles per frame is realistic, if a dynamic background grid is involved.
 */

#define IMAGE_W 16
#define IMAGE_H 16

//#define OVERRIDE_ALPHAMODE RB_ALPHAMODE_OPAQUE

#define spritec 100
static struct sprite {
  int x,y; // top left
  int dx,dy;
  struct rb_image *image;
} spritev[spritec]={0};

static struct rb_image *bgbits;

static void demo_manysprites_quit() {
  struct sprite *sprite=spritev;
  int i=spritec;
  for (;i-->0;sprite++) {
    rb_image_del(sprite->image);
  }
}

static int sprite_init(struct sprite *sprite,int nonce) {
  if (!sprite->image) {
    struct rb_image *image=spritev[nonce&3].image;
    if (rb_image_ref(image)<0) return -1;
    sprite->image=image;
  }
  sprite->x=rand()%(RB_FB_W-sprite->image->w);
  sprite->y=rand()%(RB_FB_H-sprite->image->h);
  while (!sprite->dx) sprite->dx=rand()%5-2;
  while (!sprite->dy) sprite->dy=rand()%5-2;
  return 0;
}

static struct rb_image *generate_image_0() {
  struct rb_image *image=rb_image_new(IMAGE_W,IMAGE_H);
  if (!image) return 0;
  #ifdef OVERRIDE_ALPHAMODE
    image->alphamode=OVERRIDE_ALPHAMODE;
  #endif
  int midx=image->w>>1;
  int midy=image->h>>1;
  uint32_t *p=image->pixels;
  int y=0; for (;y<image->h;y++) {
    int x=0; for (;x<image->w;x++,p++) {
      int dx=x-midx,dy=y-midy;
      double distance=sqrt(dx*dx+dy*dy)/midx;
      if (distance>1.0) {
        // leave transparent
      } else if (distance>0.900) {
        // black outline
        *p=0xff000000;
      } else {
        // yellow interior
        *p=0xffffff00;
      }
    }
  }
  int eyey=image->h/3;
  int mouthy=eyey*2;
  int eyelx=image->w/3;
  int eyerx=eyelx*2;
  int mouthx=image->w>>1;
  image->pixels[eyey*image->w+eyelx]=0xff000080;
  image->pixels[eyey*image->w+eyerx]=0xff000080;
  image->pixels[mouthy*image->w+mouthx-2]=0xff800000;
  image->pixels[mouthy*image->w+mouthx-1]=0xff800000;
  image->pixels[mouthy*image->w+mouthx]=0xff800000;
  image->pixels[mouthy*image->w+mouthx+1]=0xff800000;
  image->pixels[mouthy*image->w+mouthx+2]=0xff800000;
  return image;
}

static struct rb_image *generate_image_1() {
  struct rb_image *image=rb_image_new(IMAGE_W,IMAGE_H);
  if (!image) return 0;
  #ifdef OVERRIDE_ALPHAMODE
    image->alphamode=OVERRIDE_ALPHAMODE;
  #endif
  int midx=image->w>>1;
  int midy=image->h>>1;
  uint32_t *p=image->pixels;
  int y=0; for (;y<image->h;y++) {
    int x=0; for (;x<image->w;x++,p++) {
      int dx=x-midx,dy=y-midy;
      double distance=sqrt(dx*dx+dy*dy)/midx;
      uint8_t a;
      if (distance>=0.950) a=0x00;
      else if (distance<0.500) a=0xff;
      else a=(0.950-distance)*0.450*255.0;
      *p=(a<<24)|0x008000;
    }
  }
  return image;
}

static struct rb_image *generate_image_2() {
  struct rb_image *image=rb_image_new(IMAGE_W,IMAGE_H);
  if (!image) return 0;
  #ifdef OVERRIDE_ALPHAMODE
    image->alphamode=OVERRIDE_ALPHAMODE;
  #endif
  int midx=image->w>>1;
  int midy=image->h>>1;
  uint32_t *p=image->pixels;
  int y=0; for (;y<image->h;y++) {
    int x=0; for (;x<image->w;x++,p++) {
      if ((x&1)==(y&1)) {
        // half of pixels fully transparent
      } else {
        int dx=x-midx,dy=y-midy;
        double distance=sqrt(dx*dx+dy*dy)/midx;
        uint8_t r=(y*0xff)/image->h;
        uint8_t g=(x*0xff)/image->w;
        uint8_t b=(distance*0xff)/1.5;
        *p=0xff000000|(r<<16)|(g<<8)|b;
      }
    }
  }
  return image;
}

static struct rb_image *generate_image_3() {
  struct rb_image *image=rb_image_new(IMAGE_W,IMAGE_H);
  if (!image) return 0;
  #ifdef OVERRIDE_ALPHAMODE
    image->alphamode=OVERRIDE_ALPHAMODE;
  #endif
  int midx=image->w>>1;
  int midy=image->h>>1;
  uint32_t *p=image->pixels;
  int y=0; for (;y<image->h;y++) {
    int x=0; for (;x<image->w;x++,p++) {
      int dx=x-midx,dy=y-midy;
      double distance=sqrt(dx*dx+dy*dy)/midx;
      uint8_t r=0x80-(y*0xff)/image->h;
      uint8_t g=0x80-(x*0xff)/image->w;
      uint8_t b=(distance*0xff)/1.5;
      uint8_t a=0xff-b;
      *p=(a<<24)|(r<<16)|(g<<8)|b;
    }
  }
  return image;
}

static void draw_bgbits() {
  uint32_t *p=bgbits->pixels;
  int y=0; for (;y<RB_FB_H;y++) {
    uint8_t luma=0x40+((RB_FB_H-y)*0x80)/RB_FB_H;
    int x=0; for (;x<RB_FB_W;x++,p++) {
      uint8_t r,g,b;
      if ((x&1)==(y&1)) {
        r=luma;
        g=luma+0x20;
        b=luma-0x20;
      } else {
        r=luma+0x20;
        g=luma-0x20;
        b=luma;
      }
      *p=(r<<16)|(g<<8)|b;
    }
  }
}

static int demo_manysprites_init() {

  // Create images for the first 4 sprites (unless we're set up for fewer than that).
  if (spritec>=1) {
    if (!(spritev[0].image=generate_image_0())) return -1;
    if (spritec>=2) {
      if (!(spritev[1].image=generate_image_1())) return -1;
      if (spritec>=3) {
        if (!(spritev[2].image=generate_image_2())) return -1;
        if (spritec>=4) {
          if (!(spritev[3].image=generate_image_3())) return -1;
        }
      }
    }
  }

  struct sprite *sprite=spritev;
  int i=spritec;
  for (;i-->0;sprite++) {
    if (sprite_init(sprite,i)<0) return -1;
  }
  
  if (!(bgbits=rb_framebuffer_new())) return -1;
  draw_bgbits();
  
  fprintf(stderr,
    "Running demo with %d sprites of %dx%d pixels each.\n",
    spritec,IMAGE_W,IMAGE_H
  );
  return 0;
}

static int sprite_update(struct sprite *sprite) {
  sprite->x+=sprite->dx;
  if (sprite->x<0) {
    if (sprite->dx<0) sprite->dx=-sprite->dx;
  } else if (sprite->x>RB_FB_W-sprite->image->w) {
    if (sprite->dx>0) sprite->dx=-sprite->dx;
  }
  sprite->y+=sprite->dy;
  if (sprite->y<0) {
    if (sprite->dy<0) sprite->dy=-sprite->dy;
  } else if (sprite->y>RB_FB_H-sprite->image->h) {
    if (sprite->dy>0) sprite->dy=-sprite->dy;
  }
  return 0;
}

static int sprite_draw(struct sprite *sprite) {
  return rb_image_blit_safe(
    rb_demo_fb,sprite->x,sprite->y,
    sprite->image,0,0,
    sprite->image->w,sprite->image->h,
    RB_XFORM_NONE,0,0
  );
}

static int demo_manysprites_update() {
  // We're mixing up the model update and rendering. Bad practice in real life!
  memcpy(rb_demo_fb->pixels,bgbits->pixels,RB_FB_SIZE_BYTES);
  struct sprite *sprite=spritev;
  int i=spritec;
  for (;i-->0;sprite++) {
    if (sprite_update(sprite)<0) return -1;
    if (sprite_draw(sprite)<0) return -1;
  }
  return 1;
}

RB_DEMO(manysprites)
