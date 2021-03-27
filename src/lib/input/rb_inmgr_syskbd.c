#include "rabbit/rb_internal.h"
#include "rabbit/rb_inmgr.h"
#include "rabbit/rb_input.h"

/* Private input driver type.
 */
 
struct rb_input_syskbd {
  struct rb_input hdr;
  int connected;
};

#define INPUT ((struct rb_input_syskbd*)input)

/* Update.
 */
 
static int _rb_syskbd_update(struct rb_input *input) {
  if (!INPUT->connected) {
    if (input->delegate.cb_connect) {
      if (input->delegate.cb_connect(input,1)<0) return -1;
    }
    INPUT->connected=1;
  }
  return 0;
}

/* Device description.
 */
 
static int _rb_syskbd_device_get_description(
  struct rb_input_device_description *desc,
  struct rb_input *input,
  int devix
) {
  desc->vid=0;
  desc->pid=0;
  desc->name="System Keyboard";
  desc->namec=15;
  return 0;
}

/* Enumerate buttons.
 */
 
static int _rb_syskbd_device_enumerate(
  struct rb_input *input,
  int devix,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
) {
  int btnid,err;
  // 04..7f: All the likely keys except modifiers, and a smattering of oddballs.
  for (btnid=4;btnid<0x80;btnid++) {
    if (err=cb(0x00070000|btnid,0x00070000|btnid,0,0,2,userdata)) return err;
  }
  // e0..e7: All the modifiers.
  for (btnid=0xe0;btnid<=0xe7;btnid++) {
    if (err=cb(0x00070000|btnid,0x00070000|btnid,0,0,2,userdata)) return err;
  }
  return 0;
}

/* Driver type definition.
 */
 
static const struct rb_input_type rb_input_type_syskbd={
  .name="syskbd",
  .objlen=sizeof(struct rb_input_syskbd),
  .update=_rb_syskbd_update,
  .device_get_description=_rb_syskbd_device_get_description,
  .device_enumerate=_rb_syskbd_device_enumerate,
};

/* Get the attached keyboard driver.
 */
 
static struct rb_input *rb_inmgr_get_syskbd(struct rb_inmgr *inmgr) {
  int i=inmgr->inputc;
  while (i-->0) {
    if (inmgr->inputv[i]->type==&rb_input_type_syskbd) {
      return inmgr->inputv[i];
    }
  }
  return 0;
}

/* Attach system keyboard.
 */
 
int rb_inmgr_use_system_keyboard(struct rb_inmgr *inmgr) {
  if (rb_inmgr_get_syskbd(inmgr)) return 0; // already attached
  if (rb_inmgr_connect(inmgr,&rb_input_type_syskbd)<0) return -1;
  return 0;
}

/* Send event from system keyboard.
 */
 
int rb_inmgr_system_keyboard_event(struct rb_inmgr *inmgr,int keycode,int value) {
  struct rb_input *input=rb_inmgr_get_syskbd(inmgr);
  if (!input||!input->delegate.cb_event) return 0;
  // We could bypass some of the pipeline here, but I feel it's neater to simulate events from the very start.
  return input->delegate.cb_event(input,1,keycode,value);
}
