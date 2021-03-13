#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"

/* Global synth node type registry.
 */
 
static const struct rb_synth_node_type *rb_synth_node_typev[256]={
#define _(tag) [RB_SYNTH_NTID_##tag]=&rb_synth_node_type_##tag,
  _(noop)
  _(instrument)
  _(beep)
  _(gain)
  _(osc)
  _(env)
  _(add)
  _(mlt)
  _(fm)
#undef _
};

/* Get type from registry.
 */

const struct rb_synth_node_type *rb_synth_node_type_by_id(uint8_t ntid) {
  return rb_synth_node_typev[ntid];
}

const struct rb_synth_node_type *rb_synth_node_type_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  const struct rb_synth_node_type **p=rb_synth_node_typev;
  int i=256;
  for (;i-->0;p++) {
    if (!*p) continue;
    if (memcmp((*p)->name,name,namec)) continue;
    if ((*p)->name[namec]) continue;
    return *p;
  }
  return 0;
}

const struct rb_synth_node_type *rb_synth_node_type_by_index(int ix) {
  if (ix<0) return 0;
  const struct rb_synth_node_type **p=rb_synth_node_typev;
  int i=256;
  for (;i-->0;p++) {
    if (!*p) continue;
    if (!ix--) return *p;
  }
  return 0;
}

/* Install type.
 */

int rb_synth_node_type_install(const struct rb_synth_node_type *type,int clobber) {
  if (rb_synth_node_type_validate(type)<0) return -1;
  const struct rb_synth_node_type **dst=rb_synth_node_typev+type->ntid;
  if (*dst==type) return 0;
  if (*dst&&!clobber) return -1;
  *dst=type;
  return 0;
}

/* Validate type.
 */
 
int rb_synth_node_type_validate(const struct rb_synth_node_type *type) {
  if (!type) return -1;
  if (!type->name||!type->name[0]) return -1;
  if (type->config_objlen<(int)sizeof(struct rb_synth_node_config)) return -1;
  if (type->runner_objlen<(int)sizeof(struct rb_synth_node_runner)) return -1;
  if (!type->runner_init) return -1;
  
  if (type->fieldc<0) return -1;
  if (type->fieldc>0) {
    if (!type->fieldv) return -1;
    int fldidmin=1; // fldid zero illegal
    const struct rb_synth_node_field *field=type->fieldv;
    int i=type->fieldc;
    for (;i-->0;field++) {
      if (field->fldid<fldidmin) return -1;
      fldidmin=field->fldid+1;
      if (!field->name||!field->name[0]) return -1;
      
      if (field->flags&RB_SYNTH_NODE_FIELD_REQUIRED) {
        // REQUIRED fields may only have id 1..32
        if ((field->fldid<1)||(field->fldid>32)) return -1;
      }
      
      // Offsets must be in the custom zone.
      if (field->config_offseti) {
        if (field->config_offseti<(int)sizeof(struct rb_synth_node_config)) return -1;
        if (field->config_offseti>type->config_objlen-sizeof(int)) return -1;
      }
      if (field->config_offsetf) {
        if (field->config_offsetf<(int)sizeof(struct rb_synth_node_config)) return -1;
        if (field->config_offsetf>type->config_objlen-sizeof(rb_sample_t)) return -1;
      }
      if (field->runner_offsetf) {
        if (field->runner_offsetf<(int)sizeof(struct rb_synth_node_runner)) return -1;
        if (field->runner_offsetf>type->runner_objlen-sizeof(rb_sample_t)) return -1;
      }
      if (field->runner_offsetv) {
        if (field->runner_offsetv<(int)sizeof(struct rb_synth_node_runner)) return -1;
        if (field->runner_offsetv>type->runner_objlen-sizeof(void*)) return -1;
      }
    }
  }
  
  return 0;
}

/* Get field from type.
 */

const struct rb_synth_node_field *rb_synth_node_field_by_id(const struct rb_synth_node_type *type,uint8_t fldid) {
  int lo=0,hi=type->fieldc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rb_synth_node_field *field=type->fieldv+ck;
         if (fldid<field->fldid) hi=ck;
    else if (fldid>field->fldid) lo=ck+1;
    else return field;
  }
  return 0;
}

const struct rb_synth_node_field *rb_synth_node_field_by_name(const struct rb_synth_node_type *type,const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  const struct rb_synth_node_field *field=type->fieldv;
  int i=type->fieldc;
  for (;i-->0;field++) {
    if (memcmp(field->name,name,namec)) continue;
    if (field->name[namec]) continue;
    return field;
  }
  return 0;
}
