#include "rb_evdev_internal.h"

/* Cleanup.
 */
 
void rb_evdev_device_cleanup(struct rb_evdev_device *device) {
  if (device->fd>=0) close(device->fd);
}

static void _rb_evdev_del(struct rb_input *input) {
  if (INPUT->root) free(INPUT->root);
  if (INPUT->fd>=0) close(INPUT->fd);
  if (INPUT->pollfdv) free(INPUT->pollfdv);
  if (INPUT->devicev) {
    while (INPUT->devicec-->0) {
      rb_evdev_device_cleanup(INPUT->devicev+INPUT->devicec);
    }
    free(INPUT->devicev);
  }
}

/* Initialize.
 */
 
static int _rb_evdev_init(struct rb_input *input) {
  INPUT->fd=-1;
  INPUT->devid_next=1;
  if (rb_evdev_set_root(input,"/dev/input/",11)<0) return -1;
  return 0;
}

/* Update.
 */
 
static int _rb_evdev_update(struct rb_input *input) {

  if (rb_evdev_drop_defunct_devices(input)<0) return -1;

  if (INPUT->refresh) {
    if (rb_evdev_scan(input)<0) return -1;
    INPUT->refresh=0;
  }

  int pollfdc=rb_evdev_rebuild_pollfdv(input);
  if (pollfdc<=0) return pollfdc;
  
  int err=poll(INPUT->pollfdv,pollfdc,0);
  if (err<=0) return 0; // Eat errors (eg interrupted by signal, whatever).
  
  struct pollfd *pollfd=INPUT->pollfdv;
  int i=pollfdc;
  for (;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (pollfd->fd==INPUT->fd) {
      if (rb_evdev_update_inotify(input)<0) return -1;
    } else {
      if (rb_evdev_update_device(input,pollfd->fd)<0) return -1;
    }
  }
  
  return 0;
}

/* Device id vs index.
 */
 
static int _rb_evdev_devix_from_devid(struct rb_input *input,int devid) {
  int i=INPUT->devicec;
  const struct rb_evdev_device *device=INPUT->devicev+i-1;
  for (;i-->0;device--) {
    if (device->devid==devid) return i;
  }
  return -1;
}

static int _rb_evdev_devid_from_devix(struct rb_input *input,int devix) {
  if ((devix<0)||(devix>=INPUT->devicec)) return -1;
  return INPUT->devicev[devix].devid;
}

/* Other device searches.
 */
 
struct rb_evdev_device *rb_evdev_device_by_fd(struct rb_input *input,int fd) {
  struct rb_evdev_device *device=INPUT->devicev;
  int i=INPUT->devicec;
  for (;i-->0;device++) {
    if (device->fd==fd) return device;
  }
  return 0;
}

struct rb_evdev_device *rb_evdev_device_by_evdevid(struct rb_input *input,int evdevid) {
  struct rb_evdev_device *device=INPUT->devicev;
  int i=INPUT->devicec;
  for (;i-->0;device++) {
    if (device->evdevid==evdevid) return device;
  }
  return 0;
}

/* Device description.
 */
 
static int _rb_evdev_device_get_description(
  struct rb_input_device_description *desc,
  struct rb_input *input,
  int devix
) {
  if ((devix<0)||(devix>=INPUT->devicec)) return -1;
  memset(desc,0,sizeof(struct rb_input_device_description));
  return rb_evdev_device_get_description(desc,INPUT->devicev+devix,input);
}

/* Enumerate device buttons.
 */
 
static int _rb_evdev_device_enumerate(
  struct rb_input *input,
  int devix,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
) {
  if ((devix<0)||(devix>=INPUT->devicec)) return -1;
  return rb_evdev_device_enumerate(INPUT->devicev+devix,cb,userdata);
}

/* Disconnect device.
 */
 
static int _rb_evdev_device_disconnect(
  struct rb_input *input,
  int devix
) {
  if ((devix<0)||(devix>=INPUT->devicec)) return -1;
  struct rb_evdev_device *device=INPUT->devicev+devix;
  rb_evdev_device_cleanup(device);
  INPUT->devicec--;
  memmove(device,device+1,sizeof(struct rb_evdev_device)*(INPUT->devicec-devix));
  return 0;
}

/* Type definition.
 */
 
static struct rb_input_evdev _rb_evdev_singleton={0};

const struct rb_input_type rb_input_type_evdev={
  .name="evdev",
  .desc="Linux input via evdev.",
  .objlen=sizeof(struct rb_input_evdev),
  .singleton=&_rb_evdev_singleton,
  .del=_rb_evdev_del,
  .init=_rb_evdev_init,
  .update=_rb_evdev_update,
  .devix_from_devid=_rb_evdev_devix_from_devid,
  .devid_from_devix=_rb_evdev_devid_from_devix,
  .device_get_description=_rb_evdev_device_get_description,
  .device_enumerate=_rb_evdev_device_enumerate,
  .device_disconnect=_rb_evdev_device_disconnect,
};
