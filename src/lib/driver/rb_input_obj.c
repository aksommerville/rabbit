#include "rabbit/rb_internal.h"
#include "rabbit/rb_input.h"

/* New.
 */
 
struct rb_input *rb_input_new(
  const struct rb_input_type *type,
  const struct rb_input_delegate *delegate
) {
  if (!type) return 0;
  
  struct rb_input *input;
  if (type->singleton) {
    input=type->singleton;
    if (input->refc) return 0;
  } else {
    if (!(input=calloc(1,type->objlen))) return 0;
  }
  input->type=type;
  input->refc=1;
  
  if (delegate) {
    memcpy(&input->delegate,delegate,sizeof(struct rb_input_delegate));
  }
  
  if (type->init) {
    if (type->init(input)<0) {
      rb_input_del(input);
      return 0;
    }
  }
  
  return input;
}

/* Delete.
 */

void rb_input_del(struct rb_input *input) {
  if (!input) return;
  if (input->refc-->1) return;
  if (input->type->del) input->type->del(input);
  if (input->type->singleton) memset(input,0,input->type->objlen);
  else free(input);
}

/* Retain.
 */
 
int rb_input_ref(struct rb_input *input) {
  if (!input) return -1;
  if (input->refc<1) return -1;
  if (input->refc==INT_MAX) return -1;
  input->refc++;
  return 0;
}

/* Wrappers.
 */

int rb_input_update(struct rb_input *input) {
  return input->type->update(input);
}

int rb_input_devix_from_devid(struct rb_input *input,int devid) {
  if (!input->type->devix_from_devid) return devid;
  return input->type->devix_from_devid(input,devid);
}

int rb_input_devid_from_devix(struct rb_input *input,int devix) {
  if (!input->type->devid_from_devix) return devix;
  return input->type->devid_from_devix(input,devix);
}

int rb_input_device_get_description(
  struct rb_input_device_description *desc,
  struct rb_input *input,
  int devix
) {
  if (!input->type->device_get_description) return -1;
  return input->type->device_get_description(desc,input,devix);
}

int rb_input_device_enumerate(
  struct rb_input *input,
  int devix,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
) {
  if (!input->type->device_enumerate) return 0;
  return input->type->device_enumerate(input,devix,cb,userdata);
}

int rb_input_device_disconnect(struct rb_input *input,int devix) {
  if (!input->type->device_disconnect) return -1;
  return input->type->device_disconnect(input,devix);
}
