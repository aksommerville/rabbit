#define RB_DEMO_USE_CB_KEY 1
#include "rb_demo.h"
#include "rabbit/rb_vmgr.h"
#include "rabbit/rb_inmgr.h"
#include "rabbit/rb_archive.h"
#include "rabbit/rb_grid.h"
#include "rabbit/rb_sprite.h"

static struct rb_vmgr *vmgr=0;
static struct rb_inmgr *inmgr=0;
static int cameradstx=0;
static int cameradsty=0;

/* The same camera strategy can be trivially adjusted to do half-screens or full-screens.
 * (or pretty much any other slicing interval).
 * After observing both, I'd say this is a no-brainer: This is exactly how cameras should work for a Zelda-like game.
 */
#define FULLSCREEN 1
#define HALFSCREEN 2
#define CONTINUOUS 3
#define INTERVAL FULLSCREEN

/* Update camera.
 * This is what we're really here for.
 */
 
static int demo_halfscroll_update_camera() {
  if (vmgr->sprites->c<1) return 0;
  struct rb_sprite *sprite=vmgr->sprites->v[0];
  int xlimit=vmgr->grid->w*16-RB_FB_W;
  int ylimit=vmgr->grid->h*16-RB_FB_H;

  #if INTERVAL==CONTINUOUS
  vmgr->scrollx=sprite->x-(RB_FB_W>>1);
  if (vmgr->scrollx>xlimit) vmgr->scrollx=xlimit;
  if (vmgr->scrollx<0) vmgr->scrollx=0;
  vmgr->scrolly=sprite->y-(RB_FB_H>>1);
  if (vmgr->scrolly>ylimit) vmgr->scrolly=ylimit;
  if (vmgr->scrolly<0) vmgr->scrolly=0;
  return 0;
  #endif

  // Check hero against the target region and begin scrolling if he leaves the middle.
  // Margins must be no wider than a quarter screen, otherwise it will get stuck.
  #if INTERVAL==HALFSCREEN
    const int xmargin=16*4; // <=4
    const int ymargin=16*1; // <3
    const int xstep=RB_FB_W>>1;
    const int ystep=RB_FB_H>>1;
    const int xspeed=4;
    const int yspeed=4;
  #else
    const int xmargin=0;
    const int ymargin=0;
    const int xstep=RB_FB_W;
    const int ystep=RB_FB_H;
    const int xspeed=6;
    const int yspeed=6;
  #endif
  int viewx=sprite->x-cameradstx;
  int viewy=sprite->y-cameradsty;
  if (viewx<xmargin) {
    cameradstx-=xstep;
    if (cameradstx<0) cameradstx=0;
  } else if (viewx>RB_FB_W-xmargin) {
    cameradstx+=xstep;
    if (cameradstx>xlimit) cameradstx=xlimit;
  }
  if (viewy<ymargin) {
    cameradsty-=ystep;
    if (cameradsty<0) cameradsty=0;
  } else if (viewy>RB_FB_H-ymargin) {
    cameradsty+=ystep;
    if (cameradsty>ylimit) cameradsty=ylimit;
  }
  
  // Effect any in-progress scroll.
  if (vmgr->scrollx<cameradstx-xspeed) vmgr->scrollx+=xspeed;
  else if (vmgr->scrollx>cameradstx+xspeed) vmgr->scrollx-=xspeed;
  else vmgr->scrollx=cameradstx;
  if (vmgr->scrolly<cameradsty-yspeed) vmgr->scrolly+=yspeed;
  else if (vmgr->scrolly>cameradsty+yspeed) vmgr->scrolly-=yspeed;
  else vmgr->scrolly=cameradsty;

  return 0;
}

/* Hero sprite.
 */
 
struct hero_sprite {
  struct rb_sprite hdr;
  int dx,dy;
};

#define SPRITE ((struct hero_sprite*)sprite)

static int _hero_sprite_init(struct rb_sprite *sprite) {
  sprite->imageid=1; // scratch sprites
  sprite->tileid=0x10; // Descendant of Erdrick.
  sprite->x=RB_FB_W>>1;
  sprite->y=RB_FB_H>>1;
  return 0;
}

static const struct rb_sprite_type hero_sprite_type={
  .name="hero",
  .objlen=sizeof(struct hero_sprite),
  .init=_hero_sprite_init,
};

static int hero_sprite_update(struct rb_sprite *sprite) {

  const int speed=2;
  sprite->x+=SPRITE->dx*speed;
  sprite->y+=SPRITE->dy*speed;
  if (sprite->x<8) sprite->x=8;
  else if (sprite->x>vmgr->grid->w*16-8) sprite->x=vmgr->grid->w*16-8;
  if (sprite->y<8) sprite->y=8;
  else if (sprite->y>vmgr->grid->h*16-8) sprite->y=vmgr->grid->h*16-8;
  
  int a=(sprite->y*0xff)/(vmgr->grid->h*16);
  if (a<0) a=0;
  else if (a>0xff) a=0xff;

  return 0;
}

/* Generate a grid and install it in vmgr.
 */
 
static int demo_halfscroll_init_grid() {
  struct rb_grid *grid=rb_grid_new(64,36); // 4x4 screens of 16x9 tiles of 16x16 pixels
  if (!grid) return -1;
  if (rb_vmgr_set_grid(vmgr,grid)<0) {
    rb_grid_del(grid);
    return -1;
  }
  rb_grid_del(grid);
  
  grid->imageid=2;
  
  // Fill the border with trees 0x20
  int x,y;
  for (x=0;x<grid->w;x++) {
    grid->v[x]=0x20;
    grid->v[grid->w*(grid->h-1)+x]=0x20;
  }
  for (y=0;y<grid->h;y++) {
    grid->v[y*grid->w]=0x20;
    grid->v[(y+1)*grid->w-1]=0x20;
  }
  
  // Put a square of four stones at each full-screen corner 0x30
  for (y=8;y<grid->h-1;y+=9) {
    for (x=15;x<grid->w-1;x+=16) {
      uint8_t *p=grid->v+y*grid->w+x;
      p[0]=0x30;
      p[1]=0x30;
      p[grid->w]=0x30;
      p[grid->w+1]=0x30;
    }
  }
  
  // Add some more trees and stones randomly, to give it that Natural Feeling.
  int i=100;
  while (i-->0) {
    x=rand()%grid->w;
    y=rand()%grid->h;
    uint8_t *p=grid->v+y*grid->w+x;
    if (!*p) *p=(i&1)?0x20:0x30;
  }
  
  return 0;
}

/* Generate the hero sprite and install it in vmgr.
 */
 
static int demo_halfscroll_init_sprites() {
  struct rb_sprite *sprite=rb_sprite_new(&hero_sprite_type);
  if (!sprite) return -1;
  if (rb_vmgr_add_sprite(vmgr,sprite)<0) {
    rb_sprite_del(sprite);
    return -1;
  }
  rb_sprite_del(sprite);
  return 0;
}

/* Generic ops...
 **************************************************/

static int demo_halfscroll_inmgr_event(struct rb_inmgr *inmgr,const struct rb_input_event *event) {
  if (vmgr->sprites->c<1) return 0;
  struct rb_sprite *sprite=vmgr->sprites->v[0];
  if (event->value) switch (event->btnid) {
    case RB_BTNID_UP: SPRITE->dy=-1; break;
    case RB_BTNID_DOWN: SPRITE->dy=1; break;
    case RB_BTNID_LEFT: SPRITE->dx=-1; break;
    case RB_BTNID_RIGHT: SPRITE->dx=1; break;
  } else switch (event->btnid) {
    case RB_BTNID_UP: if (SPRITE->dy<0) SPRITE->dy=0; break;
    case RB_BTNID_DOWN: if (SPRITE->dy>0) SPRITE->dy=0; break;
    case RB_BTNID_LEFT: if (SPRITE->dx<0) SPRITE->dx=0; break;
    case RB_BTNID_RIGHT: if (SPRITE->dx>0) SPRITE->dx=0; break;
  }
  return 0;
}

static int demo_halfscroll_cb_key(int keycode,int value) {
  return rb_inmgr_system_keyboard_event(inmgr,keycode,value);
}

static int demo_halfscroll_res(
  uint32_t restype,int resid,const void *src,int srcc,void *userdata
) {
  switch (restype) {
    case RB_RES_TYPE_imag: if (rb_vmgr_set_image_serial(vmgr,resid,src,srcc)<0) return -1; break;
    case RB_RES_TYPE_song: break;
    case RB_RES_TYPE_snth: if (rb_synth_load_program(rb_demo_synth,resid,src,srcc)<0) return -1; break;
  }
  return 0;
}

static int demo_halfscroll_init() {

  if (!(vmgr=rb_vmgr_new())) return -1;
  
  struct rb_inmgr_delegate indelegate={
    .cb_event=demo_halfscroll_inmgr_event,
  };
  if (!(inmgr=rb_inmgr_new(&indelegate))) return -1;
  if (rb_inmgr_connect_all(inmgr)<0) return -1;
  if (rb_inmgr_use_system_keyboard(inmgr)<0) return -1;
  
  if (rb_archive_read("out/data",demo_halfscroll_res,0)<0) return -1;
  
  if (demo_halfscroll_init_grid()<0) return -1;
  if (demo_halfscroll_init_sprites()<0) return -1;

  return 0;
}

static void demo_halfscroll_quit() {
  rb_vmgr_del(vmgr);
  rb_inmgr_del(inmgr);
}

static int demo_halfscroll_update() {
  if (rb_inmgr_update(inmgr)<0) return -1;
  if (vmgr->sprites->c>=1) {
    if (hero_sprite_update(vmgr->sprites->v[0])<0) return -1;
  }
  if (demo_halfscroll_update_camera()<0) return -1;
  if (!(rb_demo_override_fb=rb_vmgr_render(vmgr))) return -1;
  return 1;
}

RB_DEMO(halfscroll)
