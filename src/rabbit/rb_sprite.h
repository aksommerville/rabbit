/* rb_sprite.h
 */
 
#ifndef RB_SPRITE_H
#define RB_SPRITE_H

struct rb_sprite;
struct rb_sprite_type;
struct rb_sprite_group;

/* Base sprite instance.
 ****************************************************/
 
struct rb_sprite {
  const struct rb_sprite_type *type;
  int refc;
  struct rb_sprite_group **grpv;
  int grpc,grpa;
  
  int x,y; // center in world space
  uint8_t imageid,tileid,xform; // default single-tile if no render hook
  int layer; // for sorting; higher layers render later. ties break on (y)
};

struct rb_sprite *rb_sprite_new(
  const struct rb_sprite_type *type
);

void rb_sprite_del(struct rb_sprite *sprite);
int rb_sprite_ref(struct rb_sprite *sprite);

/* Sprite type.
 *****************************************************/
 
struct rb_sprite_type {
  const char *name;
  int objlen;
  
  void (*del)(struct rb_sprite *sprite);
  int (*init)(struct rb_sprite *sprite);
  
  int (*render)(struct rb_framebuffer *dst,struct rb_sprite *sprite,int x,int y);
};

/* Sprite group.
 *****************************************************/
 
#define RB_SPRITE_GROUP_ORDER_ADDR       0 /* Sorted by address always; default. */
#define RB_SPRITE_GROUP_ORDER_EXPLICIT   1 /* Retain order of addition; re-add moves it to the back. */
#define RB_SPRITE_GROUP_ORDER_RENDER     2 /* Render order, best-effort */
#define RB_SPRITE_GROUP_ORDER_SINGLE     3 /* May contain only zero or one sprites; adding drops the prior one */
 
struct rb_sprite_group {
  struct rb_sprite **v;
  int c,a;
  int refc;
  int order; // Do not modify.
  int sortd;
};

/* In general, one should rb_sprite_group_clear() before deleting.
 * Otherwise, any sprites still present in the group will keep it alive.
 * Ask yourself "Am I deleting the group because I'm not using it anymore, or because it shouldn't exist anymore?"
 * "shouldn't exist" == clear().
 */
struct rb_sprite_group *rb_sprite_group_new(int order);
void rb_sprite_group_del(struct rb_sprite_group *group);
int rb_sprite_group_ref(struct rb_sprite_group *group);

/* Primitive single-sprite ops.
 */
int rb_sprite_group_has(const struct rb_sprite_group *group,const struct rb_sprite *sprite);
int rb_sprite_group_add(struct rb_sprite_group *group,struct rb_sprite *sprite);
int rb_sprite_group_remove(struct rb_sprite_group *group,struct rb_sprite *sprite);

/* Compound ops.
 * "kill" means remove all groups from a sprite -- this typically deletes the sprite.
 */
int rb_sprite_group_clear(struct rb_sprite_group *group);
int rb_sprite_kill(struct rb_sprite *sprite);
int rb_sprite_group_kill(struct rb_sprite_group *group);
int rb_sprite_group_add_all(struct rb_sprite_group *dst,const struct rb_sprite_group *src);
int rb_sprite_group_remove_all(struct rb_sprite_group *dst,const struct rb_sprite_group *src);

void rb_sprite_group_filter(
  struct rb_sprite_group *group,
  int (*filter)(struct rb_sprite *sprite,void *userdata),
  void *userdata
);

/* Only relevant with RB_SPRITE_GROUP_ORDER_RENDER.
 * Performs one pass of a bubble sort, putting sprites closer to render order.
 * It may take several passes to achieve the correct order.
 * We assume it's better to be out of order for a few frames than to sort exhaustively every time.
 * If you disagree, use rb_sprite_group_sort_fully() instead.
 */
void rb_sprite_group_sort(struct rb_sprite_group *group);
void rb_sprite_group_sort_fully(struct rb_sprite_group *group);

#endif
