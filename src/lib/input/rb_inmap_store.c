#include "rabbit/rb_internal.h"
#include "rabbit/rb_inmap.h"
#include "rabbit/rb_text.h"

/* New.
 */
 
struct rb_inmap_store *rb_inmap_store_new() {
  struct rb_inmap_store *store=calloc(1,sizeof(struct rb_inmap_store));
  if (!store) return 0;
  
  store->refc=1;
  
  return store;
}

/* Delete.
 */
 
void rb_inmap_store_del(struct rb_inmap_store *store) {
  if (!store) return;
  if (store->refc-->1) return;
  if (store->tmv) {
    while (store->tmc-->0) rb_inmap_template_del(store->tmv[store->tmc]);
    free(store->tmv);
  }
  free(store);
}

/* Retain.
 */
 
int rb_inmap_store_ref(struct rb_inmap_store *store) {
  if (!store) return -1;
  if (store->refc<1) return -1;
  if (store->refc==INT_MAX) return -1;
  store->refc++;
  return 0;
}

/* Match device.
 */
 
struct rb_inmap_template *rb_inmap_store_match_device(
  const struct rb_inmap_store *store,
  const struct rb_input_device_description *desc
) {
  int i=0;
  for (;i<store->tmc;i++) {
    struct rb_inmap_template *tm=store->tmv[i];
    if (rb_inmap_template_match(tm,desc)) return tm;
  }
  return 0;
}

/* Add template.
 */

int rb_inmap_store_add_template(
  struct rb_inmap_store *store,
  struct rb_inmap_template *tm
) {
  if (store->tmc>=store->tma) {
    int na=store->tma+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(store->tmv,sizeof(void*)*na);
    if (!nv) return -1;
    store->tmv=nv;
    store->tma=na;
  }
  if (rb_inmap_template_ref(tm)<0) return -1;
  store->tmv[store->tmc++]=tm;
  return 0;
}

/* Remove template.
 */

int rb_inmap_store_remove_template(
  struct rb_inmap_store *store,
  struct rb_inmap_template *tm
) {
  int i=store->tmc;
  while (i-->0) {
    if (store->tmv[i]==tm) {
      store->tmc--;
      memmove(store->tmv+i,store->tmv+i+1,sizeof(void*)*(store->tmc-i));
      rb_inmap_template_del(tm);
      return 0;
    }
  }
  return -1;
}

/* Remove all templates.
 */

int rb_inmap_store_clear(struct rb_inmap_store *store) {
  while (store->tmc>0) {
    store->tmc--;
    rb_inmap_template_del(store->tmv[store->tmc]);
  }
  return 0;
}

/* Decode template header, create and add new template.
 */
 
static struct rb_inmap_template *rb_inmap_store_decode_header(
  struct rb_inmap_store *store,
  const char *src,int srcc
) {
  int srcp=0,tokenc=0;
  const char *token;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  // First the keyword "device".
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((tokenc!=6)||memcmp(token,"device",6)) return 0;
  
  // vid
  int vid;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (rb_int_eval(&vid,token,tokenc)<0) return 0;
  if (vid&~0xffff) return 0;
  
  // pid
  int pid;
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (rb_int_eval(&pid,token,tokenc)<0) return 0;
  if (vid&~0xffff) return 0;
  
  // name
  if (srcp>=srcc) return 0;
  if (src[srcp++]!='"') return 0;
  const char *name=src+srcp;
  int namec=0;
  while (1) {
    if (srcp>=srcc) return 0;
    if (src[srcp++]=='"') break;
    namec++;
  }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  if (srcp<srcc) return 0;
  
  struct rb_inmap_template *tm=rb_inmap_template_new();
  if (!tm) return 0;
  
  if (namec) {
    if (!(tm->name=malloc(namec+1))) {
      rb_inmap_template_del(tm);
      return 0;
    }
    memcpy(tm->name,name,namec);
    tm->name[namec]=0;
    tm->namec=namec;
  }
  tm->vid=vid;
  tm->pid=pid;
  
  if (rb_inmap_store_add_template(store,tm)<0) {
    rb_inmap_template_del(tm);
    return 0;
  }
  rb_inmap_template_del(tm);
  
  return tm;
}

/* Decode.
 */
 
int rb_inmap_store_decode(struct rb_inmap_store *store,const void *src,int srcc) {
  if (!src) return 0;
  const char *SRC=src;
  if (srcc<0) { srcc=0; while (SRC[srcc]) srcc++; }
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)SRC[srcp]<=0x20) { srcp++; continue; }
    const char *line=SRC+srcp;
    int linec=0,comment=0;
    while ((srcp<srcc)&&(SRC[srcp]!=0x0a)) {
      if (SRC[srcp]=='#') comment=1;
      else if (!comment) linec++;
      srcp++;
    }
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec++;
    if (!linec) continue;
    
    struct rb_inmap_template *tm=rb_inmap_store_decode_header(store,line,linec);
    if (!tm) {
      fprintf(stderr,"ERROR: Failed to decode input template beginning: '%.*s'\n",linec,line);
      return -1;
    }
    int err=rb_inmap_template_decode(tm,SRC+srcp,srcc-srcp);
    if (err<1) return -1;
    srcp+=err;
  }
  return 0;
}

/* Encode.
 */
 
int rb_inmap_store_encode(struct rb_encoder *dst,const struct rb_inmap_store *store) {
  int i=0;
  for (;i<store->tmc;i++) {
    struct rb_inmap_template *tm=store->tmv[i];
    if (rb_inmap_template_encode(dst,tm)<0) return -1;
  }
  return 0;
}
