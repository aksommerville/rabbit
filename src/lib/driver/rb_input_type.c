#include "rabbit/rb_internal.h"
#include "rabbit/rb_input.h"

/* Global type registry.
 */
 
extern const struct rb_input_type rb_input_type_evdev;

static const struct rb_input_type *rb_input_typev[]={
#if RB_USE_evdev
  &rb_input_type_evdev,
#endif
};
 
/* Get type from registry.
 */

const struct rb_input_type *rb_input_type_by_name(const char *name,int namec) {
  if (!name) return 0;
  if (namec<0) { namec=0; while (name[namec]) namec++; }
  const struct rb_input_type **type=rb_input_typev;
  int i=sizeof(rb_input_typev)/sizeof(void*);
  for (;i-->0;type++) {
    if (memcmp((*type)->name,name,namec)) continue;
    if ((*type)->name[namec]) continue;
    return *type;
  }
  return 0;
}

const struct rb_input_type *rb_input_type_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(rb_input_typev)/sizeof(void*);
  if (p>=c) return 0;
  return rb_input_typev[p];
}

/* Validate type.
 */

int rb_input_type_validate(const struct rb_input_type *type) {
  if (!type) return -1;
  if (!type->name) return -1;
  if (!type->name[0]) return -1;
  if (type->objlen<(int)sizeof(struct rb_input)) return -1;
  if (!type->update) return -1;
  
  // If they implement one devid/devix lookup, they must implement both.
  if (type->devid_from_devix||type->devix_from_devid) {
    if (!type->devid_from_devix||!type->devix_from_devid) return -1;
  }
  
  return 0;
}
