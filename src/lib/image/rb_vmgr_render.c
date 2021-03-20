#include "rabbit/rb_internal.h"
#include "rabbit/rb_vmgr.h"
#include "rabbit/rb_grid.h"
#include "rabbit/rb_sprite.h"

/* Fill framebuffer with black.
 */
 
static void rb_vmgr_color_background(struct rb_vmgr *vmgr) {
  memset(vmgr->fb.v,0,RB_FB_SIZE_BYTES);
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
  
  //TODO Optimization: Keep a grid image, larger than the framebuffer, and redraw tiles only when the view moves.
  
  // Determine which cells need drawn.
  int cola=vmgr->scrollx/colw;
  int rowa=vmgr->scrolly/rowh;
  int colz=(vmgr->scrollx+RB_FB_W-1)/colw;
  int rowz=(vmgr->scrolly+RB_FB_H-1)/rowh;
  if (cola<0) cola=0;
  if (colz>=vmgr->grid->w) colz=vmgr->grid->w-1;
  if (cola>colz) return;
  if (rowa<0) rowa=0;
  if (rowz>=vmgr->grid->h) rowz=vmgr->grid->h-1;
  if (rowa>rowz) return;
  
  // Draw tiles.
  const uint8_t *cellrowv=vmgr->grid->v+rowa*vmgr->grid->w+cola;
  int dstx0=cola*colw-vmgr->scrollx;
  int dsty=rowa*rowh-vmgr->scrolly;
  int row=rowa; for (;row<=rowz;row++,dsty+=rowh,cellrowv+=vmgr->grid->w) {
    const uint8_t *cellv=cellrowv;
    int dstx=dstx0;
    int col=colz; for (;col<=colz;col++,dstx+=colw,cellv++) {
      int srcx=((*cellv)&15)*colw;
      int srcy=((*cellv)>>4)*rowh;
      rb_framebuffer_blit_safe(
        &vmgr->fb,dstx,dsty,
        tilesheet,srcx,srcy,
        colw,rowh,
        0,0,0
      );
    }
  }
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
      if (sprite->type->render(&vmgr->fb,sprite,x,y)<0) return -1;
    } else {
      rb_vmgr_render_tile(vmgr,sprite->imageid,sprite->tileid,sprite->xform,x,y);
    }
  }
  return 0;
}

/* Render, main entry point.
 */
 
struct rb_framebuffer *rb_vmgr_render(struct rb_vmgr *vmgr) {
  if (!vmgr) return 0;
  rb_vmgr_render_background(vmgr);
  rb_sprite_group_sort(vmgr->sprites);
  if (rb_vmgr_render_sprites(vmgr)<0) return 0;
  return &vmgr->fb;
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
  
  return rb_framebuffer_blit_safe(
    &vmgr->fb,dstx,dsty,
    src,srcx,srcy,
    colw,rowh,
    xform,0,0
  );
}
