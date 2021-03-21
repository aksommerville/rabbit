#include "rb_demo.h"
#include "rabbit/rb_vmgr.h"
#include "rabbit/rb_archive.h"
#include "rabbit/rb_grid.h"
#include "rabbit/rb_sprite.h"
#include "rabbit/rb_synth_event.h"

static struct rb_vmgr *vmgr=0;
static int dx=1,dy=1;

static int demo_vmgr_cb_res(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  //fprintf(stderr,"%s %08x:%d %d\n",__func__,restype,resid,srcc);
  switch (restype) {
  
    case RB_RES_TYPE_imag: {
        struct rb_image *image=rb_image_new_decode(src,srcc);
        if (!image) {
          fprintf(stderr,"Failed to decode imag:%d, %d bytes encoded\n",resid,srcc);
          return -1;
        }
        int err=rb_vmgr_set_image(vmgr,resid,image);
        rb_image_del(image);
        return err;
      }
    
    // In real life, you would lock the synth while loading or changing song.
    // I'm not bothering here, whatever.
    case RB_RES_TYPE_snth: {
        if (rb_synth_load_program(rb_demo_synth,resid,src,srcc)<0) return -1;
      } break;
      
    case RB_RES_TYPE_song: {
        if (resid==1) {
          struct rb_song *song=rb_song_new(src,srcc,rb_demo_synth->rate);
          if (!song) return -1;
          if (rb_synth_play_song(rb_demo_synth,song,0)<0) return -1;
        }
      } break;
    
    case RB_RES_TYPE_grid: break;//TODO
  }
  return 0;
}

static int grid_range_is_all_zero_or_sixteen(const struct rb_grid *grid,int x,int y,int w,int h) {
  if ((x<0)||(w<1)||(x>grid->w-w)) return 0;
  if ((y<0)||(h<1)||(y>grid->h-h)) return 0;
  const uint8_t *row=grid->v+y*grid->w+x;
  for (;h-->0;row+=grid->w) {
    const uint8_t *q=row;
    int xi=w;
    for (;xi-->0;q++) {
      if (*q==0x00) continue;
      if (*q==0x10) continue;
      return 0;
    }
  }
  return 1;
}

static void place_random_house(struct rb_grid *grid) {
  // Tiles 0x01..0x26 (6 columns, 3 rows)
  int x=rand()%(grid->w-6);
  int y=rand()%(grid->h-3);
  if (grid_range_is_all_zero_or_sixteen(grid,x,y,6,3)) {
    uint8_t *dst=grid->v+y*grid->w+x;
    dst[ 0]=0x01;
    dst[ 1]=0x02;
    dst[ 2]=0x03;
    dst[ 3]=0x04;
    dst[ 4]=0x05;
    dst[ 5]=0x06;
    dst[40]=0x11;
    dst[41]=0x12;
    dst[42]=0x13;
    dst[43]=0x14;
    dst[44]=0x15;
    dst[45]=0x16;
    dst[80]=0x21;
    dst[81]=0x22;
    dst[82]=0x23;
    dst[83]=0x24;
    dst[84]=0x25;
    dst[85]=0x26;
  }
}

struct my_sprite {
  struct rb_sprite hdr;
  double fx,fy;
  double dfx,dfy;
};

#define SPRITE ((struct my_sprite*)sprite)

static const struct rb_sprite_type my_sprite_type={
  .name="my_sprite",
  .objlen=sizeof(struct my_sprite),
};

static int make_random_sprite() {
  struct rb_sprite *sprite=rb_sprite_new(&my_sprite_type);
  if (!sprite) return -1;
  
  sprite->x=rand()%(vmgr->grid->w*16);
  sprite->y=rand()%(vmgr->grid->h*16);
  SPRITE->dfx=((rand()&15)-7)/7.0;
  SPRITE->dfy=((rand()&15)-7)/7.0;
  SPRITE->fx=sprite->x;
  SPRITE->fy=sprite->y;
  
  sprite->imageid=1;
  sprite->tileid=(rand()&1)?0x00:0x01;
  sprite->xform=(rand()&7);
  
  if (rb_vmgr_add_sprite(vmgr,sprite)<0) return -1;
  rb_sprite_del(sprite);
  return 0;
}

static int demo_vmgr_init() {
  if (!(vmgr=rb_vmgr_new())) return -1;
  rb_demo_override_fb=&vmgr->fb;
  
  if (rb_archive_read("out/data",demo_vmgr_cb_res,0)<0) {
    fprintf(stderr,"Failed processing archive\n");
    return -1;
  }
  
  struct rb_grid *grid=rb_grid_new(40,30);
  if (!grid) return -1;
  grid->imageid=2;
  { uint8_t *dst=grid->v;
    int row=grid->h;
    while (row-->0) {
      int col=grid->w;
      for (;col-->0;dst++) {
        if ((row&1)==(col&1)) *dst=0x10;
      }
    }
  }
  {
    int i=10;
    while (i-->0) {
      place_random_house(grid);
    }
  }
  if (rb_vmgr_set_grid(vmgr,grid)<0) return -1;
  rb_grid_del(grid);
  
  {
    int i=200;
    while (i-->0) {
      if (make_random_sprite()<0) return -1;
    }
  }
  
  return 0;
}

static void demo_vmgr_quit() {
  rb_vmgr_del(vmgr);
  vmgr=0;
}

static void update_my_sprite(struct rb_sprite *sprite) {
  SPRITE->fx+=SPRITE->dfx;
  if (SPRITE->fx<0.0) {
    if (SPRITE->dfx<0.0) SPRITE->dfx=-SPRITE->dfx;
  } else if (SPRITE->fx>40*16) {
    if (SPRITE->dfx>0.0) SPRITE->dfx=-SPRITE->dfx;
  }
  SPRITE->fy+=SPRITE->dfy;
  if (SPRITE->fy<0.0) {
    if (SPRITE->dfy<0.0) SPRITE->dfy=-SPRITE->dfy;
  } else if (SPRITE->fy>30*16) {
    if (SPRITE->dfy>0.0) SPRITE->dfy=-SPRITE->dfy;
  }
  sprite->x=SPRITE->fx;
  sprite->y=SPRITE->fy;
}

static int demo_vmgr_update() {

  vmgr->scrollx+=dx;
  if (vmgr->scrollx<0) {
    if (dx<0) dx=-dx;
  } else if (vmgr->scrollx>40*16-RB_FB_W) {
    if (dx>0) dx=-dx;
  }
  vmgr->scrolly+=dy;
  if (vmgr->scrolly<0) {
    if (dy<0) dy=-dy;
  } else if (vmgr->scrolly>30*16-RB_FB_H) {
    if (dy>0) dy=-dy;
  }
  
  int i=vmgr->sprites->c;
  while (i-->0) {
    update_my_sprite(vmgr->sprites->v[i]);
  }

  if (rb_vmgr_render(vmgr)!=rb_demo_override_fb) {
    fprintf(stderr,"rb_vmgr_render() failed\n");
    return -1;
  }
  return 1;
}

RB_DEMO(vmgr)
