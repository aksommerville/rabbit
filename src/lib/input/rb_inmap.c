#include "rabbit/rb_internal.h"
#include "rabbit/rb_inmap.h"
#include "rabbit/rb_inmgr.h"

/* New.
 */
 
struct rb_inmap *rb_inmap_new() {
  struct rb_inmap *inmap=calloc(1,sizeof(struct rb_inmap));
  if (!inmap) return 0;
  inmap->refc=1;
  return inmap;
}

/* Delete.
 */
 
void rb_inmap_del(struct rb_inmap *inmap) {
  if (!inmap) return;
  if (inmap->refc-->1) return;
  if (inmap->fieldv) free(inmap->fieldv);
  free(inmap);
}

/* Retain.
 */
 
int rb_inmap_ref(struct rb_inmap *inmap) {
  if (!inmap) return -1;
  if (inmap->refc<1) return -1;
  if (inmap->refc==INT_MAX) return -1;
  inmap->refc++;
  return 0;
}

/* Update state and trigger callback.
 */
 
static int rb_inmap_player_event(struct rb_inmap *inmap,int btnid,int value) {
  if (!(btnid&~0xffff)) {
    if (value) inmap->state|=btnid;
    else inmap->state&=~btnid;
  }
  if (inmap->cb) {
    if (inmap->cb(inmap,btnid,value)<0) return -1;
  }
  return 0;
}

/* Turn a hat value (0..7) into (dx,dy).
 * OOB is normal, that means (0,0).
 */
 
static void rb_inmap_split_hat(int *dx,int *dy,int v) {
  switch (v) {
    case 0: *dx=0; *dy=-1; break;
    case 1: *dx=1; *dy=-1; break;
    case 2: *dx=1; *dy=0; break;
    case 3: *dx=1; *dy=1; break;
    case 4: *dx=0; *dy=1; break;
    case 5: *dx=-1; *dy=1; break;
    case 6: *dx=-1; *dy=0; break;
    case 7: *dx=-1; *dy=-1; break;
    default: *dx=0; *dy=0;
  }
}

/* Process event.
 */
 
int rb_inmap_event(struct rb_inmap *inmap,int btnid,int value) {
  int p=rb_inmap_search(inmap,btnid);
  if (p<0) return 0;
  struct rb_inmap_field *field=inmap->fieldv+p;
  for (;(p<inmap->fieldc)&&(field->srcbtnid==btnid);p++,field++) {
    if (value==field->srcvalue) continue;
    
    // Hats work a little different...
    if (field->dstbtnid==RB_BTNID_DPAD) {
      int px,py,nx,ny;
      rb_inmap_split_hat(&px,&py,field->srcvalue-field->srclo);
      rb_inmap_split_hat(&nx,&ny,value-field->srclo);
      if (px!=nx) {
        if (px<0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_LEFT,0)<0) return -1;
        } else if (px>0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_RIGHT,0)<0) return -1;
        }
        if (nx<0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_LEFT,1)<0) return -1;
        } else if (nx>0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_RIGHT,1)<0) return -1;
        }
      }
      if (py!=ny) {
        if (py<0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_UP,0)<0) return -1;
        } else if (py>0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_DOWN,0)<0) return -1;
        }
        if (ny<0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_UP,1)<0) return -1;
        } else if (ny>0) {
          if (rb_inmap_player_event(inmap,RB_BTNID_DOWN,1)<0) return -1;
        }
      }
      return 0;
    }
    
    // Ordinary two-state and three-way buttons...
    field->srcvalue=value;
    int dstvalue=((value>=field->srclo)&&(value<=field->srchi))?1:0;
    if (dstvalue==field->dstvalue) continue;
    field->dstvalue=dstvalue;
    if (rb_inmap_player_event(inmap,field->dstbtnid,dstvalue)<0) return -1;
  }
  return 0;
}

/* Search fields.
 */
 
int rb_inmap_search(const struct rb_inmap *inmap,int srcbtnid) {
  int lo=0,hi=inmap->fieldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (srcbtnid<inmap->fieldv[ck].srcbtnid) hi=ck;
    else if (srcbtnid>inmap->fieldv[ck].srcbtnid) lo=ck+1;
    else {
      while ((ck>lo)&&(inmap->fieldv[ck-1].srcbtnid==srcbtnid)) ck--;
      return ck;
    }
  }
  return -lo-1;
}

/* Add field.
 */
 
struct rb_inmap_field *rb_inmap_insert(struct rb_inmap *inmap,int p,int srcbtnid) {
  if (p<0) {
    p=rb_inmap_search(inmap,srcbtnid);
    if (p<0) p=-p-1;
  }
  if ((p<0)||(p>inmap->fieldc)) return 0;
  if (p&&(srcbtnid<inmap->fieldv[p-1].srcbtnid)) return 0;
  if ((p<inmap->fieldc)&&(srcbtnid>inmap->fieldv[p].srcbtnid)) return 0;
  
  if (inmap->fieldc>=inmap->fielda) {
    int na=inmap->fielda+16;
    if (na>INT_MAX/sizeof(struct rb_inmap_field)) return 0;
    void *nv=realloc(inmap->fieldv,sizeof(struct rb_inmap_field)*na);
    if (!nv) return 0;
    inmap->fieldv=nv;
    inmap->fielda=na;
  }
  
  struct rb_inmap_field *field=inmap->fieldv+p;
  memmove(field+1,field,sizeof(struct rb_inmap_field)*(inmap->fieldc-p));
  inmap->fieldc++;
  memset(field,0,sizeof(struct rb_inmap_field));
  field->srcbtnid=srcbtnid;
  return field;
}

/* Get all output buttons.
 */
 
uint16_t rb_inmap_get_mapped_buttons(const struct rb_inmap *inmap) {
  uint16_t buttons=0;
  const struct rb_inmap_field *field=inmap->fieldv;
  int i=inmap->fieldc;
  for (;i-->0;field++) {
    if (field->dstbtnid&~0xffff) continue;
    buttons|=field->dstbtnid;
  }
  return buttons;
}
