#include "rb_demo.h"
#include "rabbit/rb_vmgr.h"
#include "rabbit/rb_archive.h"
#include "rabbit/rb_grid.h"
#include "rabbit/rb_sprite.h"

static struct rb_vmgr *vmgr=0;

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
    
    case RB_RES_TYPE_snth: break;//TODO
    case RB_RES_TYPE_song: break;//TODO
    case RB_RES_TYPE_grid: break;//TODO
  }
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
  grid->v[41]=0x01;
  grid->v[42]=0x02;
  grid->v[43]=0x03;
  grid->v[44]=0x04;
  grid->v[45]=0x05;
  grid->v[46]=0x06;
  grid->v[81]=0x11;
  grid->v[82]=0x12;
  grid->v[83]=0x13;
  grid->v[84]=0x14;
  grid->v[85]=0x15;
  grid->v[86]=0x16;
  grid->v[121]=0x21;
  grid->v[122]=0x22;
  grid->v[123]=0x23;
  grid->v[124]=0x24;
  grid->v[125]=0x25;
  grid->v[126]=0x26;
  if (rb_vmgr_set_grid(vmgr,grid)<0) return -1;
  rb_grid_del(grid);
  
  return 0;
}

static void demo_vmgr_quit() {
  rb_vmgr_del(vmgr);
  vmgr=0;
}

static int demo_vmgr_update() {
  if (rb_vmgr_render(vmgr)!=rb_demo_override_fb) {
    fprintf(stderr,"rb_vmgr_render() failed\n");
    return -1;
  }
  return 1;
}

RB_DEMO(vmgr)
