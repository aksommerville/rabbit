#include "rabbit/rb_internal.h"
#include "rabbit/rb_vmgr.h"
#include "rabbit/rb_grid.h"
#include "rabbit/rb_sprite.h"
 
/* Fill framebuffer with black.
 */
 
static void rb_vmgr_color_background(struct rb_vmgr *vmgr) {
  memset(vmgr->fb->pixels,0,RB_FB_SIZE_BYTES);
}

/* Redraw bgbits from scratch.
 */
 
static inline void rb_vmgr_refresh_bgbits(
  struct rb_vmgr *vmgr,
  struct rb_image *tilesheet,
  int colw,int rowh,
  int worldw,int worldh
) {

  // Line up bgbits on a cell boundary within world limits, containing the current view.
  // It will never exceed the left or top world bounds but may exceed right or bottom (if the world is tiny).
  vmgr->bgbitsx=vmgr->scrollx-colw;
  if (vmgr->bgbitsx>worldw-vmgr->bgbits->w) vmgr->bgbitsx=worldw-vmgr->bgbits->w;
  if (vmgr->bgbitsx<0) vmgr->bgbitsx=0;
  vmgr->bgbitsx-=vmgr->bgbitsx%colw;
  vmgr->bgbitsy=vmgr->scrolly-rowh;
  if (vmgr->bgbitsy>worldh-vmgr->bgbits->h) vmgr->bgbitsy=worldh-vmgr->bgbits->h;
  if (vmgr->bgbitsy<0) vmgr->bgbitsy=0;
  vmgr->bgbitsy-=vmgr->bgbitsy%rowh;
  
  int cola=vmgr->bgbitsx/colw;
  int colz=(vmgr->bgbitsx+vmgr->bgbits->w-1)/colw;
  if (colz>=vmgr->grid->w) colz=vmgr->grid->w-1;
  int rowa=vmgr->bgbitsy/rowh;
  int rowz=(vmgr->bgbitsy+vmgr->bgbits->h-1)/rowh;
  if (rowz>=vmgr->grid->h) rowz=vmgr->grid->h-1;
  
  const uint8_t *src=vmgr->grid->v+rowa*vmgr->grid->w+cola;
  int dsty=0,row=rowa;
  for (;row<=rowz;row++,src+=vmgr->grid->w,dsty+=rowh) {
    const uint8_t *p=src;
    int dstx=0,col=cola;
    for (;col<=colz;col++,p++,dstx+=colw) {
      int srcx=((*p)&15)*colw;
      int srcy=((*p)>>4)*rowh;
      rb_image_blit_safe(
        vmgr->bgbits,dstx,dsty,
        tilesheet,srcx,srcy,
        colw,rowh,
        0,0,0
      );
    }
  }
}

/* Check whether bgbits still contains the view.
 * If not, make it so.
 */
 
static inline void rb_vmgr_update_bgbits(
  struct rb_vmgr *vmgr,
  struct rb_image *tilesheet,
  int colw,int rowh,
  int worldw,int worldh
) {
  //TODO We could optimize this even further with rb_image_scroll(), and draw only the necessary part.
  // I feel like that's overkill; copying opaque tiles is pretty cheap.
  // Our main interest here is don't redraw the grid while the camera stands still.
  // Experimentally, this not-quite-optimal approach reduces load about 7x in a worst-case test.
  // I estimate balls-to-the-wall optimization would yield more like 10x.
  if (
    (vmgr->scrollx<vmgr->bgbitsx)||
    (vmgr->scrolly<vmgr->bgbitsy)||
    (vmgr->scrollx+RB_FB_W>vmgr->bgbitsx+vmgr->bgbits->w)||
    (vmgr->scrolly+RB_FB_H>vmgr->bgbitsy+vmgr->bgbits->h)
  ) {
    rb_vmgr_refresh_bgbits(vmgr,tilesheet,colw,rowh,worldw,worldh);
  }
}

/* Background: Grid or black.
 */
 
static void rb_vmgr_render_background(struct rb_vmgr *vmgr) {

  // No grid or no tilesheet, black out and return.
  struct rb_image *tilesheet=0;
  if (vmgr->grid&&(vmgr->grid->imageid<RB_VMGR_IMAGE_COUNT)) {
    tilesheet=vmgr->imagev[vmgr->grid->imageid];
  }
  if (!tilesheet) {
    rb_vmgr_color_background(vmgr);
    return;
  }
  
  // Take some measurements.
  int colw=tilesheet->w>>4;
  int rowh=tilesheet->h>>4;
  int worldw=vmgr->grid->w*colw;
  int worldh=vmgr->grid->h*rowh;
  
  // If our view exceeds the world bounds or tiles have transparency, black out first.
  if (
    (vmgr->scrollx<0)||(vmgr->scrollx>worldw-RB_FB_W)||
    (vmgr->scrolly<0)||(vmgr->scrolly>worldh-RB_FB_H)||
    (tilesheet->alphamode!=RB_ALPHAMODE_OPAQUE)
  ) {
    rb_vmgr_color_background(vmgr);
  }
  
  // If our view exceeds bgbits, or if forced, refresh it.
  if (vmgr->bgbitsdirty) {
    rb_vmgr_refresh_bgbits(vmgr,tilesheet,colw,rowh,worldw,worldh);
    vmgr->bgbitsdirty=0;
  } else {
    rb_vmgr_update_bgbits(vmgr,tilesheet,colw,rowh,worldw,worldh);
  }
  
  // Copy from bgbits.
  rb_image_blit_safe(
    vmgr->fb,0,0,
    vmgr->bgbits,vmgr->scrollx-vmgr->bgbitsx,vmgr->scrolly-vmgr->bgbitsy,
    RB_FB_W,RB_FB_H,
    0,0,0
  );
}

/* Foreground: Sprites.
 */
 
static int rb_vmgr_render_sprites(struct rb_vmgr *vmgr) {
  int i=0;
  for (;i<vmgr->sprites->c;i++) {
    struct rb_sprite *sprite=vmgr->sprites->v[i];
    int x=sprite->x-vmgr->scrollx;
    int y=sprite->y-vmgr->scrolly;
    
    if (sprite->type->render) {
      if (sprite->type->render(vmgr->fb,sprite,x,y)<0) return -1;
    } else {
      rb_vmgr_render_tile(vmgr,sprite->imageid,sprite->tileid,sprite->xform,x,y);
    }
  }
  return 0;
}

/* Render, main entry point.
 */
 
struct rb_image *rb_vmgr_render(struct rb_vmgr *vmgr) {
  if (!vmgr) return 0;
  
  rb_vmgr_render_background(vmgr);
  rb_sprite_group_sort(vmgr->sprites);
  if (rb_vmgr_render_sprites(vmgr)<0) return 0;
  
  return vmgr->fb;
}

/* Render tile.
 */
 
int rb_vmgr_render_tile(
  struct rb_vmgr *vmgr,
  uint8_t imageid,uint8_t tileid,uint8_t xform,
  int x,int y
) {
  if (imageid>=RB_VMGR_IMAGE_COUNT) return -1;
  struct rb_image *src=vmgr->imagev[imageid];
  if (!src) return -1;
  
  int colw=src->w>>4;
  int rowh=src->h>>4;
  int col=tileid&15;
  int row=tileid>>4;
  int srcx=col*colw;
  int srcy=row*rowh;
  int dstx=x-(colw>>1);
  int dsty=y-(rowh>>1);
  
  return rb_image_blit_safe(
    vmgr->fb,dstx,dsty,
    src,srcx,srcy,
    colw,rowh,
    xform,0,0
  );
}
