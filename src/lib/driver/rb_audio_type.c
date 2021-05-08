#include "rabbit/rb_internal.h"
#include "rabbit/rb_audio.h"

/* Audio type registry.
 */
 
extern const struct rb_audio_type rb_audio_type_pulse;
 
static const struct rb_audio_type *rb_audio_typev[]={
#if RB_USE_pulse
  &rb_audio_type_pulse,
#endif
};

/* Get type by name.
 */
 
const struct rb_audio_type *rb_audio_type_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  int i=sizeof(rb_audio_typev)/sizeof(void*);
  const struct rb_audio_type **p=rb_audio_typev;
  for (;i-->0;p++) {
    if (memcmp((*p)->name,name,namec)) continue;
    if ((*p)->name[namec]) continue;
    return *p;
  }
  return 0;
}

/* Get type by index.
 */
 
const struct rb_audio_type *rb_audio_type_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(rb_audio_typev)/sizeof(void*);
  if (p>=c) return 0;
  return rb_audio_typev[p];
}

/* Validate.
 */

int rb_audio_type_validate(const struct rb_audio_type *type) {
  if (!type) return -1;
  if (!type->name||!type->name[0]) return -1;
  if (type->objlen<(int)sizeof(struct rb_audio)) return -1;
  
  // One of (update) or (lock,unlock) must be implemented.
  if (type->update) {
  } else if (type->lock&&type->unlock) {
  } else return -1;
  
  return 0;
}
