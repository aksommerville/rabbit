/* rb_vmgr.h
 * High-level video manager, responsible for rendering the world scene.
 *  - Keeps a store of up to 256 source images, like tilesheets.
 *  - Background grid.
 *  - Foreground sprites.
 */
 
#ifndef RB_VMGR_H
#define RB_VMGR_H

struct rb_image;
struct rb_grid;
struct rb_sprite;
struct rb_sprite_group;

#include "rb_image.h"

#define RB_VMGR_IMAGE_COUNT 256

struct rb_vmgr {
  int refc;
  struct rb_grid *grid;
  struct rb_sprite_group *sprites;
  int scrollx,scrolly;
  struct rb_image *imagev[RB_VMGR_IMAGE_COUNT];
  struct rb_image *fb;
  struct rb_image *bgbits; // 32 pixels wider and taller than the framebuffer, grid image
  int bgbitsx,bgbitsy;
  int bgbitsdirty; // nonzero to redraw bgbits from scratch
};

struct rb_vmgr *rb_vmgr_new();
void rb_vmgr_del(struct rb_vmgr *vmgr);
int rb_vmgr_ref(struct rb_vmgr *vmgr);

int rb_vmgr_set_image(struct rb_vmgr *vmgr,uint8_t imageid,struct rb_image *image);
int rb_vmgr_set_image_serial(struct rb_vmgr *vmgr,uint8_t imageid,const void *src,int srcc);

/* Render one frame.
 * Returns my framebuffer on success or null on error.
 * Caller should deliver this framebuffer to the video driver.
 * You can add overlay content before that, of course.
 */
struct rb_image *rb_vmgr_render(struct rb_vmgr *vmgr);

int rb_vmgr_set_grid(struct rb_vmgr *vmgr,struct rb_grid *grid);
int rb_vmgr_set_sprites(struct rb_vmgr *vmgr,struct rb_sprite_group *group);

int rb_vmgr_add_sprite(struct rb_vmgr *vmgr,struct rb_sprite *sprite);
int rb_vmgr_remove_sprite(struct rb_vmgr *vmgr,struct rb_sprite *sprite);

/* Keep all sprites for which (filter) returns nonzero.
 */
void rb_vmgr_filter_sprites(
  struct rb_vmgr *vmgr,
  int (*filter)(struct rb_sprite *sprite,void *userdata),
  void *userdata
);

/* Of interest to sprite render hooks.
 * Unless noted with "_world", these take framebuffer coordinates.
 */
int rb_vmgr_render_tile(struct rb_vmgr *vmgr,uint8_t imageid,uint8_t tileid,uint8_t xform,int x,int y);

#endif
