#include "rabbit/rb_internal.h"
#include "rabbit/rb_sprite.h"

/* New.
 */
 
struct rb_sprite *rb_sprite_new(
  const struct rb_sprite_type *type
) {
  if (!type) return 0;
  if (type->objlen<(int)sizeof(struct rb_sprite)) return 0;
  
  struct rb_sprite *sprite=calloc(1,type->objlen);
  if (!sprite) return 0;
  
  sprite->type=type;
  sprite->refc=1;
  
  if (type->init) {
    if (type->init(sprite)<0) {
      rb_sprite_del(sprite);
      return 0;
    }
  }
  
  return sprite;
}

/* Delete.
 */

void rb_sprite_del(struct rb_sprite *sprite) {
  if (!sprite) return;
  if (sprite->refc-->0) return;
  
  if (sprite->type->del) sprite->type->del(sprite);
  
  if (sprite->grpc) {
    fprintf(stderr,"!!! Deleting sprite %p with grpc==%d, should have been zero !!!\n",sprite,sprite->grpc);
  }
  if (sprite->grpv) free(sprite->grpv);
  
  free(sprite);
}

/* Retain.
 */
 
int rb_sprite_ref(struct rb_sprite *sprite) {
  if (!sprite) return -1;
  if (sprite->refc<1) return -1;
  if (sprite->refc==INT_MAX) return -1;
  sprite->refc++;
  return 0;
}
