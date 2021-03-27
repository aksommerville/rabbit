#include "rabbit/rb_internal.h"
#include "rabbit/rb_inmgr.h"
#include "rabbit/rb_input.h"
#include "rabbit/rb_inmap.h"
#include "rabbit/rb_text.h"

/* New.
 */

struct rb_inmgr *rb_inmgr_new(const struct rb_inmgr_delegate *delegate) {
  struct rb_inmgr *inmgr=calloc(1,sizeof(struct rb_inmgr));
  if (!inmgr) return 0;
  inmgr->refc=1;
  inmgr->playerc=1;
  
  if (delegate) memcpy(&inmgr->delegate,delegate,sizeof(struct rb_inmgr_delegate));

  int i=RB_PLAYER_LIMIT+1;
  while (i-->0) inmgr->playerv[i].plrid=i;
  
  if (!(inmgr->store=rb_inmap_store_new())) {
    rb_inmgr_del(inmgr);
    return 0;
  }

  return inmgr;
}

/* Delete.
 */
 
void rb_inmgr_del(struct rb_inmgr *inmgr) {
  if (!inmgr) return;
  if (inmgr->refc-->0) return;
  if (inmgr->inputv) {
    while (inmgr->inputc-->0) rb_input_del(inmgr->inputv[inmgr->inputc]);
    free(inmgr->inputv);
  }
  if (inmgr->inmapv) {
    while (inmgr->inmapc-->0) rb_inmap_del(inmgr->inmapv[inmgr->inmapc]);
    free(inmgr->inmapv);
  }
  rb_inmap_store_del(inmgr->store);
  free(inmgr);
}

/* Retain.
 */
 
int rb_inmgr_ref(struct rb_inmgr *inmgr) {
  if (!inmgr) return -1;
  if (inmgr->refc<1) return -1;
  if (inmgr->refc==INT_MAX) return -1;
  inmgr->refc++;
  return 0;
}

/* Map is being reassigned -- simulate all of its buttons going zero.
 */
 
static int rb_inmgr_drop_map_buttons(struct rb_inmgr *inmgr,struct rb_inmap *inmap) {
  struct rb_inmap_field *field=inmap->fieldv;
  int i=inmap->fieldc;
  for (;i-->0;field++) {
    if (!field->dstvalue) continue;
    int value=field->srclo-1;
    if (field->srclo==INT_MIN) value=field->srchi+1;
    if (rb_inmap_event(inmap,field->srcbtnid,value)<0) return -1;
  }
  return 0;
}

/* Reassign devices after a connection or player count change.
 * This must detect when a map is freshly assigned or unassigned and set RB_BTNID_CD accordingly.
 * (firing events too).
 */
 
static int rb_inmgr_reassign_devices(struct rb_inmgr *inmgr) {
  fprintf(stderr,"%s...\n",__func__);
  if ((inmgr->playerc<1)||(inmgr->playerc>RB_PLAYER_LIMIT)) inmgr->playerc=1;
  
  // Unassign any devices out of range, and count assignments by plrid.
  int devc_by_plrid[1+RB_PLAYER_LIMIT]={0};
  int i=inmgr->inmapc;
  while (i-->0) {
    struct rb_inmap *map=inmgr->inmapv[i];
    if ((map->plrid<0)||(map->plrid>inmgr->playerc)) {
      if (rb_inmgr_drop_map_buttons(inmgr,map)<0) return -1;
      map->plrid=0;
    }
    devc_by_plrid[map->plrid]++;
  }
  
  // Assign any unassigned devices. Low playerid first.
  while (devc_by_plrid[0]>0) {
    struct rb_inmap *map=0;
    for (i=inmgr->inmapc;i-->0;) {
      if (!inmgr->inmapv[i]->plrid) {
        map=inmgr->inmapv[i];
        break;
      }
    }
    if (!map) return -1;
    int loneliest=1;
    for (i=1;i<=inmgr->playerc;i++) {
      if (devc_by_plrid[i]<devc_by_plrid[loneliest]) {
        loneliest=i;
      }
    }
    if (rb_inmgr_drop_map_buttons(inmgr,map)<0) return -1;
    devc_by_plrid[map->plrid]--;
    map->plrid=loneliest;
    devc_by_plrid[map->plrid]++;
  }
  
  // While any player has no devices, and some player has multiple, reassign one.
  while (1) {
    int plrid_none=0,plrid_many=0;
    for (i=1;i<=inmgr->playerc;i++) {
      if (!devc_by_plrid[i]&&!plrid_none) plrid_none=i;
      else if ((devc_by_plrid[i]>1)&&!plrid_many) plrid_many=i;
    }
    if (!plrid_none||!plrid_many) break;
    struct rb_inmap *map=0;
    for (i=inmgr->inmapc;i-->0;) {
      if (inmgr->inmapv[i]->plrid==plrid_many) {
        map=inmgr->inmapv[i];
        break;
      }
    }
    if (!map) return -1;
    if (rb_inmgr_drop_map_buttons(inmgr,map)<0) return -1;
    devc_by_plrid[map->plrid]--;
    map->plrid=plrid_none;
    devc_by_plrid[map->plrid]++;
  }
  
  // Update RB_BTNID_CD for all players.
  for (i=1;i<=inmgr->playerc;i++) {
    if (devc_by_plrid[i]) {
      if (!(inmgr->playerv[i].state&RB_BTNID_CD)) {
        inmgr->playerv[i].state|=RB_BTNID_CD;
        struct rb_input_event event={
          .plrid=i,
          .btnid=RB_BTNID_CD,
          .value=1,
          .state=inmgr->playerv[i].state,
        };
        if (inmgr->delegate.cb_event&&(inmgr->delegate.cb_event(inmgr,&event)<0)) return -1;
      }
    } else {
      if (inmgr->playerv[i].state&RB_BTNID_CD) {
        inmgr->playerv[i].state=0;
        struct rb_input_event event={
          .plrid=i,
          .btnid=RB_BTNID_CD,
          .value=0,
          .state=0,
        };
        if (inmgr->delegate.cb_event&&(inmgr->delegate.cb_event(inmgr,&event)<0)) return -1;
      }
    }
  }
  for (;i<RB_PLAYER_LIMIT;i++) {
    if (inmgr->playerv[i].state&RB_BTNID_CD) {
      inmgr->playerv[i].state=0;
      struct rb_input_event event={
        .plrid=i,
        .btnid=RB_BTNID_CD,
        .value=0,
        .state=0,
      };
      if (inmgr->delegate.cb_event&&(inmgr->delegate.cb_event(inmgr,&event)<0)) return -1;
    }
  }
  
  return 0;
}

/* Fire callback.
 */
 
static int rb_inmgr_forward(struct rb_inmgr *inmgr,int plrid,int btnid,int value,uint16_t state) {
  if (!inmgr->delegate.cb_event) return 0;
  struct rb_input_event event={
    .plrid=plrid,
    .btnid=btnid,
    .value=value,
    .state=state,
    //TODO I neglected to pass the source event all the way up here. Do we need it? Revisit this.
  };
  return inmgr->delegate.cb_event(inmgr,&event);
}

/* Update player states and fire callback.
 */
 
static int rb_inmgr_player_event(struct rb_inmgr *inmgr,int plrid,int btnid,int value) {
  if ((plrid>=0)&&(plrid<=8)) {
    struct rb_inmgr_player *player=inmgr->playerv+plrid;
    if (btnid<0x10000) {
      if (value) {
        if (!(player->state&btnid)) {
          player->state|=btnid;
          if (rb_inmgr_forward(inmgr,plrid,btnid,value,player->state)<0) return -1;
        }
      } else {
        if (player->state&btnid) {
          player->state&=~btnid;
          if (rb_inmgr_forward(inmgr,plrid,btnid,value,player->state)<0) return -1;
        }
      }
    }
  }
  if (plrid) {
    return rb_inmgr_player_event(inmgr,0,btnid,value);
  }
  return 0;
}

/* Mapped button changed.
 */
 
static int rb_inmgr_cb_mapped(struct rb_inmap *inmap,int btnid,int value) {
  struct rb_inmgr *inmgr=inmap->userdata;
  if (rb_inmgr_player_event(inmgr,inmap->plrid,btnid,value)<0) return -1;
  return 0;
}

/* Device connected.
 */
 
static int rb_inmgr_cb_connect(struct rb_input *input,int devid) {
  struct rb_inmgr *inmgr=input->delegate.userdata;
  int devix=rb_input_devix_from_devid(input,devid);
  if (devix<0) return 0;
  
  struct rb_input_device_description desc={0};
  if (rb_input_device_get_description(&desc,input,devix)<0) {
    rb_input_device_disconnect(input,devix);
    return 0;
  }
  
  struct rb_inmap *inmap=0;
  struct rb_inmap_template *tm=rb_inmap_store_match_device(inmgr->store,&desc);
  if (!tm) {
    if (!(tm=rb_inmap_template_synthesize(input,devix,&desc,&inmap))) {
      return 0;
    }
    if (rb_inmap_store_add_template(inmgr->store,tm)<0) {
      rb_inmap_template_del(tm);
      rb_inmap_del(inmap);
      return -1;
    }
    rb_inmap_template_del(tm);
    if (!inmap) {
      return 0;
    }
  } else {
    if (!(inmap=rb_inmap_template_instantiate(tm,input,devix))) {
      return 0;
    }
  }
  
  inmap->plrid=0;
  inmap->devid=devid;
  inmap->userdata=inmgr;
  inmap->cb=rb_inmgr_cb_mapped;
  inmap->state=RB_BTNID_CD;
  
  if (rb_inmgr_add_map(inmgr,inmap)<0) {
    rb_inmap_del(inmap);
    return -1;
  }
  rb_inmap_del(inmap);
  
  if (rb_inmgr_reassign_devices(inmgr)<0) return -1;
  if (rb_inmgr_cb_mapped(inmap,RB_BTNID_CD,1)<0) return -1;

  return 0;
}

/* Device disconnected.
 */
 
static int rb_inmgr_cb_disconnect(struct rb_input *input,int devid) {
  struct rb_inmgr *inmgr=input->delegate.userdata;
  int p=rb_inmgr_search_maps(inmgr,devid);
  if (p<0) return 0;
  struct rb_inmap *inmap=inmgr->inmapv[p];
  
  if (inmap->state) {
    uint16_t btnid=1;
    for (;btnid&&(btnid<=inmap->state);btnid<<=1) {
      if (inmap->state&btnid) {
        if (rb_inmgr_cb_mapped(inmap,btnid,0)<0) return -1;
      }
    }
  }
  
  // We may have just fired a bunch of callbacks, so ensure that (p) remains valid.
  if ((p>=inmgr->inmapc)||(inmgr->inmapv[p]!=inmap)) return -1;
  inmgr->inmapc--;
  memmove(inmgr->inmapv+p,inmgr->inmapv+p+1,sizeof(void*)*(inmgr->inmapc-p));
  rb_inmap_del(inmap);
  
  return 0;
}

/* Raw event.
 */
 
static int rb_inmgr_cb_event(struct rb_input *input,int devid,int btnid,int value) {
  struct rb_inmgr *inmgr=input->delegate.userdata;
  
  int p=rb_inmgr_search_maps(inmgr,devid);
  if (p<0) {
    if (inmgr->include_raw&&inmgr->delegate.cb_event) {
      struct rb_input_event event={
        .devid=devid,
        .devbtnid=btnid,
        .devvalue=value,
      };
      return inmgr->delegate.cb_event(inmgr,&event);
    }
    return 0;
  }
  struct rb_inmap *inmap=inmgr->inmapv[p];
  
  if (rb_inmap_event(inmap,btnid,value)<0) return -1;
  return 0;
}

/* Initialize drivers.
 */
 
int rb_inmgr_connect_all(struct rb_inmgr *inmgr) {
  int p=0;
  const struct rb_input_type *type;
  for (;type=rb_input_type_by_index(p);p++) {
    if (rb_inmgr_connect(inmgr,type)<0) return -1;
  }
  return 0;
}

int rb_inmgr_connect(struct rb_inmgr *inmgr,const struct rb_input_type *type) {
  if (!inmgr||!type) return -1;
  if (inmgr->inputc>=inmgr->inputa) {
    int na=inmgr->inputa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(inmgr->inputv,sizeof(void*)*na);
    if (!nv) return -1;
    inmgr->inputv=nv;
    inmgr->inputa=na;
  }
  struct rb_input_delegate delegate={
    .userdata=inmgr,
    .cb_connect=rb_inmgr_cb_connect,
    .cb_disconnect=rb_inmgr_cb_disconnect,
    .cb_event=rb_inmgr_cb_event,
  };
  struct rb_input *input=rb_input_new(type,&delegate);
  if (!input) {
    return -1;
  }
  inmgr->inputv[inmgr->inputc++]=input;
  return 0;
}

/* Update.
 */
 
int rb_inmgr_update(struct rb_inmgr *inmgr) {
  int i=inmgr->inputc;
  while (i-->0) {
    struct rb_input *input=inmgr->inputv[i];
    if (rb_input_update(input)<0) return -1;
  }
  return 0;
}

/* Set player count.
 */
 
int rb_inmgr_set_player_count(struct rb_inmgr *inmgr,int playerc) {
  if ((playerc<1)||(playerc>RB_PLAYER_LIMIT)) return -1;
  if (playerc==inmgr->playerc) return 0;
  inmgr->playerc=playerc;
  if (rb_inmgr_reassign_devices(inmgr)<0) return -1;
  return 0;
}

/* Trivial accessors.
 */
 
uint16_t rb_inmgr_get_state(struct rb_inmgr *inmgr,int plrid) {
  if ((plrid<0)||(plrid>RB_PLAYER_LIMIT)) return 0;
  return inmgr->playerv[plrid].state;
}

/* Add map.
 */
 
int rb_inmgr_add_map(struct rb_inmgr *inmgr,struct rb_inmap *inmap) {
  int p=rb_inmgr_search_maps(inmgr,inmap->devid);
  if (p>=0) return -1;
  p=-p-1;
  if (inmgr->inmapc>=inmgr->inmapa) {
    int na=inmgr->inmapa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(inmgr->inmapv,sizeof(void*)*na);
    if (!nv) return -1;
    inmgr->inmapv=nv;
    inmgr->inmapa=na;
  }
  if (rb_inmap_ref(inmap)<0) return -1;
  memmove(inmgr->inmapv+p+1,inmgr->inmapv+p,sizeof(void*)*(inmgr->inmapc-p));
  inmgr->inmapc++;
  inmgr->inmapv[p]=inmap;
  return 0;
}

/* Search maps.
 */
 
int rb_inmgr_search_maps(struct rb_inmgr *inmgr,int devid) {
  int lo=0,hi=inmgr->inmapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (devid<inmgr->inmapv[ck]->devid) hi=ck;
    else if (devid>inmgr->inmapv[ck]->devid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Button names.
 */
 
int rb_input_button_repr(char *dst,int dsta,int btnid) {
  const char *src=0;
  int srcc=0;
  switch (btnid) {
    case RB_BTNID_LEFT: src="left"; srcc=4; break;
    case RB_BTNID_RIGHT: src="right"; srcc=5; break;
    case RB_BTNID_UP: src="up"; srcc=2; break;
    case RB_BTNID_DOWN: src="down"; srcc=4; break;
    case RB_BTNID_A: src="a"; srcc=1; break;
    case RB_BTNID_B: src="b"; srcc=1; break;
    case RB_BTNID_C: src="c"; srcc=1; break;
    case RB_BTNID_D: src="d"; srcc=1; break;
    case RB_BTNID_L: src="l"; srcc=1; break;
    case RB_BTNID_R: src="r"; srcc=1; break;
    case RB_BTNID_START: src="start"; srcc=5; break;
    case RB_BTNID_SELECT: src="select"; srcc=6; break;
  }
  if (!srcc) return rb_decsint_repr(dst,dsta,btnid);
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}

int rb_input_button_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  switch (srcc) {
    case 1: switch (src[0]) {
        case 'a': return RB_BTNID_A;
        case 'b': return RB_BTNID_B;
        case 'c': return RB_BTNID_C;
        case 'd': return RB_BTNID_D;
        case 'l': return RB_BTNID_L;
        case 'r': return RB_BTNID_R;
      } break;
    case 2: {
        if (!memcmp(src,"up",2)) return RB_BTNID_UP;
      } break;
    case 4: {
        if (!memcmp(src,"down",4)) return RB_BTNID_DOWN;
        if (!memcmp(src,"left",4)) return RB_BTNID_LEFT;
      } break;
    case 5: {
        if (!memcmp(src,"right",5)) return RB_BTNID_RIGHT;
        if (!memcmp(src,"start",5)) return RB_BTNID_START;
      } break;
    case 6: {
        if (!memcmp(src,"select",6)) return RB_BTNID_SELECT;
      } break;
  }
  int n;
  if (rb_int_eval(&n,src,srcc)>=0) {
    return n;
  }
  return -1;
}
