#include "rabbit/rb_internal.h"
#include "rabbit/rb_video.h"

/* Video type registry.
 */
 
extern const struct rb_video_type rb_video_type_glx;
extern const struct rb_video_type rb_video_type_drmgx;
extern const struct rb_video_type rb_video_type_bcm;
 
static const struct rb_video_type *rb_video_typev[]={
#if RB_USE_glx
  &rb_video_type_glx,
#endif
#if RB_USE_drmgx
  &rb_video_type_drmgx,
#endif
#if RB_USE_bcm
  &rb_video_type_bcm,
#endif
};

/* Get type by name.
 */
 
const struct rb_video_type *rb_video_type_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  int i=sizeof(rb_video_typev)/sizeof(void*);
  const struct rb_video_type **p=rb_video_typev;
  for (;i-->0;p++) {
    if (memcmp((*p)->name,name,namec)) continue;
    if ((*p)->name[namec]) continue;
    return *p;
  }
  return 0;
}

/* Get type by index.
 */
 
const struct rb_video_type *rb_video_type_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(rb_video_typev)/sizeof(void*);
  if (p>=c) return 0;
  return rb_video_typev[p];
}

/* Validate.
 */

int rb_video_type_validate(const struct rb_video_type *type) {
  if (!type) return -1;
  if (!type->name||!type->name[0]) return -1;
  if (type->objlen<(int)sizeof(struct rb_video)) return -1;
  if (!type->swap) return -1;
  return 0;
}
