#include "rabbit/rb_internal.h"
#include "rabbit/rb_sprite.h"

/* New.
 */
 
struct rb_sprite_group *rb_sprite_group_new(int order) {
  struct rb_sprite_group *group=calloc(1,sizeof(struct rb_sprite_group));
  if (!group) return 0;
  
  group->refc=1;
  group->order=order;
  group->sortd=1;
  
  return group;
}

/* Delete.
 */
 
void rb_sprite_group_del(struct rb_sprite_group *group) {
  if (!group) return;
  if (group->refc-->1) return;
  
  if (group->c) {
    fprintf(stderr,"!!! Deleting group %p with c==%d, should have been zero !!!\n",group,group->c);
  }
  if (group->v) free(group->v);
  
  free(group);
}

/* Retain.
 */
 
int rb_sprite_group_ref(struct rb_sprite_group *group) {
  if (!group) return -1;
  if (group->refc<1) return -1;
  if (group->refc==INT_MAX) return -1;
  group->refc++;
  return 0;
}

/* Find sprite in group.
 */
 
static int _rb_group_find_sprite(const struct rb_sprite_group *group,const struct rb_sprite *sprite) {
  switch (group->order) {
  
    case RB_SPRITE_GROUP_ORDER_ADDR: {
        int lo=0,hi=group->c;
        while (lo<hi) {
          int ck=(lo+hi)>>1;
          const struct rb_sprite *q=group->v[ck];
               if (sprite<q) hi=ck;
          else if (sprite>q) lo=ck=1;
          else return ck;
        }
        return -lo-1;
      }
      
    case RB_SPRITE_GROUP_ORDER_RENDER: {
        // This is the same brute-force approach as default,
        // but we'll try to return a sane insertion point when not found.
        int i=group->c,insp=0;
        while (i-->0) {
          const struct rb_sprite *q=group->v[i];
          if (q==sprite) return i;
          if (!insp) {
            if (q->layer<sprite->layer) insp=i+1;
            else if ((q->layer==sprite->layer)&&(q->y<=sprite->y)) insp=i+1;
          }
        }
        return -insp-1;
      }
      
    default: {
        int i=group->c;
        while (i-->0) {
          if (group->v[i]==sprite) return i;
        }
        return -1;
      }
  }
}

/* Find group in sprite.
 * These are always sorted by address.
 */
 
static int _rb_sprite_find_group(const struct rb_sprite *sprite,const struct rb_sprite_group *group) {
  int lo=0,hi=sprite->grpc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rb_sprite_group *q=sprite->grpv[ck];
         if (group<q) hi=ck;
    else if (group>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Private insert/remove primitives -- one side only.
 */
 
static int _rb_group_add_sprite(struct rb_sprite_group *group,int p,struct rb_sprite *sprite) {
  if ((p<0)||(p>group->c)) return -1;
  if (group->c>=group->a) {
    int na=group->a+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(group->v,sizeof(void*)*na);
    if (!nv) return -1;
    group->v=nv;
    group->a=na;
  }
  if (rb_sprite_ref(sprite)<0) return -1;
  memmove(group->v+p+1,group->v+p,sizeof(void*)*(group->c-p));
  group->c++;
  group->v[p]=sprite;
  return 0;
}

static int _rb_sprite_add_group(struct rb_sprite *sprite,int p,struct rb_sprite_group *group) {
  if ((p<0)||(p>sprite->grpc)) return -1;
  if (sprite->grpc>=sprite->grpa) {
    int na=sprite->grpa+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(sprite->grpv,sizeof(void*)*na);
    if (!nv) return -1;
    sprite->grpv=nv;
    sprite->grpa=na;
  }
  if (rb_sprite_group_ref(group)<0) return -1;
  memmove(sprite->grpv+p+1,sprite->grpv+p,sizeof(void*)*(sprite->grpc-p));
  sprite->grpc++;
  sprite->grpv[p]=group;
  return 0;
}

static void _rb_group_remove_sprite(struct rb_sprite_group *group,int p) {
  if ((p<0)||(p>=group->c)) return;
  struct rb_sprite *sprite=group->v[p];
  group->c--;
  memmove(group->v+p,group->v+p+1,sizeof(void*)*(group->c-p));
  rb_sprite_del(sprite);
}

static void _rb_sprite_remove_group(struct rb_sprite *sprite,int p) {
  if ((p<0)||(p>=sprite->grpc)) return;
  struct rb_sprite_group *group=sprite->grpv[p];
  sprite->grpc--;
  memmove(sprite->grpv+p,sprite->grpv+p+1,sizeof(void*)*(sprite->grpc-p));
  rb_sprite_group_del(group);
}

/* Test membership.
 */

int rb_sprite_group_has(const struct rb_sprite_group *group,const struct rb_sprite *sprite) {
  if (!group||!sprite) return 0;
  // Searching groups in a sprite is almost always a simpler proposition than sprites in a group.
  return (_rb_sprite_find_group(sprite,group)>=0)?1:0;
}

/* Add sprite to group -- special case for SINGLE.
 */
 
static int _rb_sprite_group_add_single(struct rb_sprite_group *group,struct rb_sprite *sprite) {
  if (group->c) {
    struct rb_sprite *prior=group->v[0];
    if (prior==sprite) return 0;
    
    int grpp=_rb_sprite_find_group(sprite,group);
    if (grpp>=0) return -1;
    grpp=-grpp-1;
    
    if (_rb_sprite_add_group(sprite,grpp,group)<0) return -1;
    if (rb_sprite_ref(sprite)<0) {
      _rb_sprite_remove_group(sprite,grpp);
      return -1;
    }
    group->v[0]=sprite;
    
    if ((grpp=_rb_sprite_find_group(prior,group))>=0) {
      _rb_sprite_remove_group(prior,grpp);
    }
    rb_sprite_del(prior);
  } else {
    int grpp=_rb_sprite_find_group(sprite,group);
    if (grpp>=0) return -1;
    grpp=-grpp-1;
    if (_rb_sprite_add_group(sprite,grpp,group)<0) return -1;
    if (_rb_group_add_sprite(group,0,sprite)<0) {
      _rb_sprite_remove_group(sprite,grpp);
      return -1;
    }
  }
  return 1;
}

/* Add sprite to group.
 */

int rb_sprite_group_add(struct rb_sprite_group *group,struct rb_sprite *sprite) {
  if (!group||!sprite) return -1;
  if (group->order==RB_SPRITE_GROUP_ORDER_SINGLE) {
    return _rb_sprite_group_add_single(group,sprite);
  }
  
  int grpp=_rb_sprite_find_group(sprite,group);
  if (grpp>=0) return 0; // already present
  grpp=-grpp-1;
  int sprp=_rb_group_find_sprite(group,sprite);
  if (sprp>=0) return -1; // Inconsistent! This is a serious error if it ever happens.
  sprp=-sprp-1;
  
  if (_rb_group_add_sprite(group,sprp,sprite)<0) return -1;
  if (_rb_sprite_add_group(sprite,grpp,group)<0) {
    _rb_group_remove_sprite(group,sprp);
    return -1;
  }
  return 1;
}

/* Remove sprite from group.
 */
  
int rb_sprite_group_remove(struct rb_sprite_group *group,struct rb_sprite *sprite) {
  if (!group||!sprite) return -1;
  int grpp=_rb_sprite_find_group(sprite,group);
  if (grpp<0) return 0; // not a member, that's cool
  int sprp=_rb_group_find_sprite(group,sprite);
  if (sprp<0) return -1; // Inconsistent!
  _rb_group_remove_sprite(group,sprp);
  _rb_sprite_remove_group(sprite,grpp);
  return 1;
}

/* Remove all sprites from group.
 */

int rb_sprite_group_clear(struct rb_sprite_group *group) {
  if (!group) return -1;
  if (group->c<1) return 0;
  if (rb_sprite_group_ref(group)<0) return -1;
  while (group->c>0) {
    group->c--;
    struct rb_sprite *sprite=group->v[group->c];
    int grpp=_rb_sprite_find_group(sprite,group);
    if (grpp>=0) {
      _rb_sprite_remove_group(sprite,grpp);
    }
    rb_sprite_del(sprite);
  }
  rb_sprite_group_del(group);
  return 0;
}

/* Remove all groups from sprite.
 */
 
int rb_sprite_kill(struct rb_sprite *sprite) {
  if (!sprite) return -1;
  if (sprite->grpc<1) return 0;
  if (rb_sprite_ref(sprite)<0) return -1;
  while (sprite->grpc>0) {
    sprite->grpc--;
    struct rb_sprite_group *group=sprite->grpv[sprite->grpc];
    int sprp=_rb_group_find_sprite(group,sprite);
    if (sprp>=0) { // looks like a fatal inconsistency, but this does happen during rb_sprite_group_kill(), it's cool
      _rb_group_remove_sprite(group,sprp);
    }
    rb_sprite_group_del(group);
  }
  rb_sprite_del(sprite);
  return 0;
}

/* Remove all groups from all sprites in group.
 */
 
int rb_sprite_group_kill(struct rb_sprite_group *group) {
  if (!group) return -1;
  if (group->c<1) return 0;
  if (rb_sprite_group_ref(group)<0) return -1;
  while (group->c>0) {
    group->c--;
    struct rb_sprite *sprite=group->v[group->c];
    rb_sprite_kill(sprite); // owing to the (group->c--) above, our reference stays alive
    rb_sprite_del(sprite);
  }
  rb_sprite_group_del(group);
  return 0;
}

/* Add multiple sprites to group.
 */
 
int rb_sprite_group_add_all(struct rb_sprite_group *dst,const struct rb_sprite_group *src) {
  if (!dst) return -1;
  if (!src) return 0;
  if (src->c<1) return 0;
  
  // If the target group is SINGLE, we can cheat and skip right to the end.
  // ...why on earth would anyone add_all into a SINGLE group? huh
  if (dst->order==RB_SPRITE_GROUP_ORDER_SINGLE) {
    return rb_sprite_group_add(dst,src->v[src->c-1]);
  }
  
  //TODO Optimization: We could do this a bit more efficiently when both groups are ORDER_ADDR.
  
  int i=0;
  for (;i<src->c;i++) {
    if (rb_sprite_group_add(dst,src->v[i])<0) return -1;
  }
  return 0;
}

/* Remove multiple sprites from group.
 */
 
int rb_sprite_group_remove_all(struct rb_sprite_group *dst,const struct rb_sprite_group *src) {
  if (!dst) return -1;
  if (!src) return 0;
  if (dst->c<1) return 0;
  
  //TODO Optimization: We could do this a bit more efficiently when both groups are ORDER_ADDR.
  
  if (rb_sprite_group_ref(dst)<0) return -1;
  int i=src->c;
  while (i-->0) {
    if (rb_sprite_group_remove(dst,src->v[i])<0) {
      rb_sprite_group_del(dst);
      return -1;
    }
  }
  rb_sprite_group_del(dst);
  return 0;
}

/* Filter with custom hook.
 */

void rb_sprite_group_filter(
  struct rb_sprite_group *group,
  int (*filter)(struct rb_sprite *sprite,void *userdata),
  void *userdata
) {
  int i=group->c;
  while (i-->0) {
    struct rb_sprite *sprite=group->v[i];
    if (!filter(sprite,userdata)) {
      group->c--;
      memmove(group->v+i,group->v+i+1,sizeof(void*)*(group->c-i));
      int grpp=_rb_sprite_find_group(sprite,group);
      if (grpp>=0) _rb_sprite_remove_group(sprite,grpp);
      rb_sprite_del(sprite);
    }
  }
}

/* Sort.
 */
 
static int rb_sprite_cmp_render(const struct rb_sprite *a,const struct rb_sprite *b) {
  if (a->layer<b->layer) return -1;
  if (a->layer>b->layer) return 1;
  if (a->y<b->y) return -1;
  if (a->y>b->y) return 1;
  return 0;
}
 
void rb_sprite_group_sort(struct rb_sprite_group *group) {
  if (group->order!=RB_SPRITE_GROUP_ORDER_RENDER) return;
  if (group->c<2) return;
  int first,last,i;
  if (group->sortd==1) {
    first=0;
    last=group->c-1;
  } else {
    first=group->c-1;
    last=0;
  }
  for (i=first;i!=last;i+=group->sortd) {
    struct rb_sprite *a=group->v[i];
    struct rb_sprite *b=group->v[i+group->sortd];
    if (rb_sprite_cmp_render(a,b)==group->sortd) {
      group->v[i]=b;
      group->v[i+group->sortd]=a;
    }
  }
  if (group->sortd==1) group->sortd=-1;
  else group->sortd=1;
}

void rb_sprite_group_sort_fully(struct rb_sprite_group *group) {
  if (group->order!=RB_SPRITE_GROUP_ORDER_RENDER) return;
  if (group->c<2) return;
  qsort(group->v,group->c,sizeof(void*),(int(*)(const void*,const void*))rb_sprite_cmp_render);
}
