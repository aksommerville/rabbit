#include "rabbit/rb_internal.h"
#include "rabbit/rb_vmgr.h"
#include "rabbit/rb_sprite.h"
#include "rabbit/rb_grid.h"

/* New.
 */

struct rb_vmgr *rb_vmgr_new() {
  struct rb_vmgr *vmgr=calloc(1,sizeof(struct rb_vmgr));
  if (!vmgr) return 0;
  
  vmgr->refc=1;
  if (!(vmgr->sprites=rb_sprite_group_new(RB_SPRITE_GROUP_ORDER_RENDER))) {
    rb_vmgr_del(vmgr);
    return 0;
  }
  
  if (!(vmgr->fb=rb_framebuffer_new())) {
    rb_vmgr_del(vmgr);
    return 0;
  }
  
  return vmgr;
}

/* Delete.
 */
 
void rb_vmgr_del(struct rb_vmgr *vmgr) {
  if (!vmgr) return;
  if (vmgr->refc-->1) return;
  
  rb_grid_del(vmgr->grid);
  rb_sprite_group_clear(vmgr->sprites);
  rb_sprite_group_del(vmgr->sprites);
  
  int i=RB_VMGR_IMAGE_COUNT;
  while (i-->0) {
    if (vmgr->imagev[i]) rb_image_del(vmgr->imagev[i]);
  }
  
  rb_image_del(vmgr->fb);
  
  free(vmgr);
}

/* Retain.
 */
 
int rb_vmgr_ref(struct rb_vmgr *vmgr) {
  if (!vmgr) return -1;
  if (vmgr->refc<1) return -1;
  if (vmgr->refc==INT_MAX) return -1;
  vmgr->refc++;
  return 0;
}

/* Install image.
 */

int rb_vmgr_set_image(struct rb_vmgr *vmgr,uint8_t imageid,struct rb_image *image) {
  if (!vmgr) return -1;
  if (imageid>=RB_VMGR_IMAGE_COUNT) return -1;
  struct rb_image **dst=vmgr->imagev+imageid;
  if (*dst==image) return 0;
  if (image&&(rb_image_ref(image)<0)) return -1;
  rb_image_del(*dst);
  *dst=image;
  return 0;
}

/* Decode and install image.
 */

int rb_vmgr_set_image_serial(struct rb_vmgr *vmgr,uint8_t imageid,const void *src,int srcc) {
  struct rb_image *image=rb_image_new_decode(src,srcc);
  if (!image) return -1;
  int err=rb_vmgr_set_image(vmgr,imageid,image);
  rb_image_del(image);
  return err;
}

/* Replace grid.
 */

int rb_vmgr_set_grid(struct rb_vmgr *vmgr,struct rb_grid *grid) {
  if (!vmgr) return -1;
  if (vmgr->grid==grid) return 0;
  if (grid&&(rb_grid_ref(grid)<0)) return -1;
  rb_grid_del(vmgr->grid);
  vmgr->grid=grid;
  return 0;
}

/* Replace sprite group.
 */
 
int rb_vmgr_set_sprites(struct rb_vmgr *vmgr,struct rb_sprite_group *group) {
  if (!vmgr||!group) return -1;
  if (vmgr->sprites==group) return 0;
  if (rb_sprite_group_ref(group)<0) return -1;
  rb_sprite_group_clear(vmgr->sprites);
  rb_sprite_group_del(vmgr->sprites);
  vmgr->sprites=group;
  return 0;
}

/* Access to sprite group.
 */

int rb_vmgr_add_sprite(struct rb_vmgr *vmgr,struct rb_sprite *sprite) {
  return rb_sprite_group_add(vmgr->sprites,sprite);
}

int rb_vmgr_remove_sprite(struct rb_vmgr *vmgr,struct rb_sprite *sprite) {
  return rb_sprite_group_remove(vmgr->sprites,sprite);
}

void rb_vmgr_filter_sprites(
  struct rb_vmgr *vmgr,
  int (*filter)(struct rb_sprite *sprite,void *userdata),
  void *userdata
) {
  rb_sprite_group_filter(vmgr->sprites,filter,userdata);
}
