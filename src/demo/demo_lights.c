#define RB_DEMO_USE_CB_KEY 1
#include "rb_demo.h"
#include "rabbit/rb_inmgr.h"
#include "rabbit/rb_vmgr.h"
#include "rabbit/rb_archive.h"
#include "rabbit/rb_grid.h"
#include "rabbit/rb_font.h"

static struct rb_inmgr *inmgr=0;
static struct rb_vmgr *vmgr=0;
static struct rb_lights lights={0};
static uint16_t instate=0;
static struct rb_image *message=0;

#define extralightc 0
static struct rb_extralight {
  int id;
  int dx,dy;
} extralightv[extralightc];

/* Receive input.
 */

static int demo_lights_inmgr_event(struct rb_inmgr *inmgr,const struct rb_input_event *event) {
  if (!event->plrid) instate=event->state;
  return 0;
}

static int demo_lights_poll_input() {
  struct rb_light *light;

  if (instate&RB_BTNID_A) { // [1] position
    if (light=rb_lights_get(&lights,1)) {
      if (instate&RB_BTNID_LEFT) light->x--;
      else if (instate&RB_BTNID_RIGHT) light->x++;
      if (instate&RB_BTNID_UP) light->y--;
      else if (instate&RB_BTNID_DOWN) light->y++;
    }
    
  } else if (instate&RB_BTNID_B) { // [1] radii
    if (light=rb_lights_get(&lights,1)) {
      if (instate&RB_BTNID_LEFT) {
        if (light->gradius>0) light->gradius--;
      } else if (instate&RB_BTNID_RIGHT) {
        light->gradius++;
      }
      if (instate&RB_BTNID_UP) {
        light->radius++;
      } else if (instate&RB_BTNID_DOWN) {
        if (light->radius>0) light->radius--;
      }
    }
    
  } else if (instate&RB_BTNID_C) { // [2] position
    if (light=rb_lights_get(&lights,2)) {
      if (instate&RB_BTNID_LEFT) light->x--;
      else if (instate&RB_BTNID_RIGHT) light->x++;
      if (instate&RB_BTNID_UP) light->y--;
      else if (instate&RB_BTNID_DOWN) light->y++;
    }
    
  } else if (instate&RB_BTNID_D) { // [2] radii
    if (light=rb_lights_get(&lights,2)) {
      if (instate&RB_BTNID_LEFT) {
        if (light->gradius>0) light->gradius--;
      } else if (instate&RB_BTNID_RIGHT) {
        light->gradius++;
      }
      if (instate&RB_BTNID_UP) {
        light->radius++;
      } else if (instate&RB_BTNID_DOWN) {
        if (light->radius>0) light->radius--;
      }
    }
  
  } else { // background
    if (instate&RB_BTNID_UP) {
      if (lights.bg<0xfd) lights.bg+=2;
      else lights.bg=0xff;
    } else if (instate&RB_BTNID_DOWN) {
      if (lights.bg>=2) lights.bg-=2;
      else lights.bg=0;
    }
  }
  return 0;
}

/* Move extralights.
 */
 
static int demo_lights_move_extralights() {
  struct rb_extralight *extralight=extralightv;
  int i=extralightc;
  for (;i-->0;extralight++) {
    struct rb_light *light=rb_lights_get(&lights,extralight->id);
    if (!light) return -1;
    light->x+=extralight->dx;
    light->y+=extralight->dy;
    if ((light->x<0)&&(extralight->dx<0)) extralight->dx=-extralight->dx;
    else if ((light->x>RB_FB_W)&&(extralight->dx>0)) extralight->dx=-extralight->dx;
    if ((light->y<0)&&(extralight->dy<0)) extralight->dy=-extralight->dy;
    else if ((light->y>RB_FB_H)&&(extralight->dy>0)) extralight->dy=-extralight->dy;
  }
  return 0;
}

/* Business-level setup.
 */
 
static int demo_lights_setup() {

  // Make two interactive lights.
  lights.bg=0x80;
  struct rb_light *light;
  if (1) {
    if (!(light=rb_lights_add(&lights,1))) return -1;
    light->x=RB_FB_W>>2;
    light->y=RB_FB_H>>1;
    light->radius=32;
    light->gradius=1;
    if (!(light=rb_lights_add(&lights,2))) return -1;
    light->x=(RB_FB_W*3)>>2;
    light->y=RB_FB_H>>1;
    light->radius=8;
    light->gradius=40;
  }
  
  // For shits and giggles make a bunch of other lights that move on their own.
  struct rb_extralight *extralight=extralightv;
  int i=extralightc;
  for (;i-->0;extralight++) {
    extralight->id=20+i;
    if (!(light=rb_lights_add(&lights,extralight->id))) return -1;
    light->x=rand()%RB_FB_W;
    light->y=rand()%RB_FB_H;
    light->radius=2;
    light->gradius=10;
    do { extralight->dx=rand()%5-2; } while (!extralight->dx);
    do { extralight->dy=rand()%5-2; } while (!extralight->dy);
  }
  
  // Make a one-screen grid filled with random selections of the top 4 tiles in the left column.
  struct rb_grid *grid=rb_grid_new(16,9);
  if (!grid) return -1;
  int err=rb_vmgr_set_grid(vmgr,grid);
  rb_grid_del(grid);
  if (err<0) return -1;
  grid->imageid=2;
  uint8_t *cell=grid->v;
  for (i=grid->w*grid->h;i-->0;cell++) {
    switch (rand()&3) {
      case 0: *cell=0x00; break;
      case 1: *cell=0x10; break;
      case 2: *cell=0x20; break;
      case 3: *cell=0x30; break;
    }
  }
  
  // Make an overlay message to explain what's going on.
  struct rb_image *font=rb_font_generate_minimal();
  if (!font) return -1;
  message=rb_font_print(
    font,RB_FONTCONTENT_G0,RB_FONT_FLAG_MARGINL|RB_FONT_FLAG_MARGINT,0,
    "Hold a thumb button to choose field, then d-pad to adjust it.",-1
  );
  rb_image_del(font);
  if (!message||(message->w>RB_FB_W)) {
    fprintf(stderr,"Failed to generate message! %p %d\n",message,message?message->w:-1);
    return -1;
  }
  message->alphamode=RB_ALPHAMODE_BLEND;
  rb_image_replace_by_alpha(message,0xff000080,0xa0ffffff);
  
  return 0;
}

/* Demo glue.
 *****************************************/

static int demo_lights_cb_key(int keycode,int value) {
  return rb_inmgr_system_keyboard_event(inmgr,keycode,value);
}

static int demo_lights_res(
  uint32_t restype,int resid,const void *src,int srcc,void *userdata
) {
  switch (restype) {
    case RB_RES_TYPE_imag: if (rb_vmgr_set_image_serial(vmgr,resid,src,srcc)<0) return -1; break;
    case RB_RES_TYPE_song: break;
    case RB_RES_TYPE_snth: if (rb_synth_load_program(rb_demo_synth,resid,src,srcc)<0) return -1; break;
  }
  return 0;
}
 
static int demo_lights_init() {

  if (!(vmgr=rb_vmgr_new())) return -1;
  
  struct rb_inmgr_delegate indelegate={
    .cb_event=demo_lights_inmgr_event,
  };
  if (!(inmgr=rb_inmgr_new(&indelegate))) return -1;
  if (rb_inmgr_connect_all(inmgr)<0) return -1;
  if (rb_inmgr_use_system_keyboard(inmgr)<0) return -1;
  
  if (rb_archive_read("out/data",demo_lights_res,0)<0) return -1;
  
  if (demo_lights_setup()<0) return -1;
  
  return 0;
}

static void demo_lights_quit() {
  rb_inmgr_del(inmgr);
  inmgr=0;
  rb_vmgr_del(vmgr);
  vmgr=0;
  rb_lights_cleanup(&lights);
  rb_image_del(message);
  message=0;
}

static int demo_lights_update() {
  if (rb_inmgr_update(inmgr)<0) return -1;
  if (demo_lights_poll_input()<0) return -1;
  if (demo_lights_move_extralights()<0) return -1;
  if (!(rb_demo_override_fb=rb_vmgr_render(vmgr))) return -1;

  if (rb_lights_draw(rb_demo_override_fb,&lights,vmgr->scrollx,vmgr->scrolly)<0) return -1;
  
  rb_image_blit_unchecked(
    rb_demo_override_fb,(rb_demo_override_fb->w>>1)-(message->w>>1),rb_demo_override_fb->h-message->h-5,
    message,0,0,
    message->w,message->h,
    0,0,0
  );
  
  return 1;
}

RB_DEMO(lights)
