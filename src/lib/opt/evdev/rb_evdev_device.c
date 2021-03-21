#include "rb_evdev_internal.h"

/* Add device.
 */
 
int rb_evdev_add_device(struct rb_input *input,int fd,int evdevid) {
  
  if (INPUT->devicec>=INPUT->devicea) {
    int na=INPUT->devicea+8;
    if (na>INT_MAX/sizeof(struct rb_evdev_device)) return -1;
    void *nv=realloc(INPUT->devicev,sizeof(struct rb_evdev_device)*na);
    if (!nv) return -1;
    INPUT->devicev=nv;
    INPUT->devicea=na;
  }
  
  struct rb_evdev_device *device=INPUT->devicev+INPUT->devicec++;
  memset(device,0,sizeof(struct rb_evdev_device));
  device->fd=fd;
  device->evdevid=evdevid;
  
  // If (devid_next) overflows, restart at one.
  // This is vanishingly unlikely, and if it happens, there's probably something else wrong. Meh.
  device->devid=INPUT->devid_next++;
  if (INPUT->devid_next<1) INPUT->devid_next=1;
  
  int err=0;
  if (input->delegate.cb_connect) {
    err=input->delegate.cb_connect(input,device->devid);
  }
  return err;
}

/* Get description.
 */
 
int rb_evdev_device_get_description(
  struct rb_input_device_description *desc,
  struct rb_evdev_device *device,
  struct rb_input *input
) {
  if (device->fd<0) return -1;
  
  struct input_id id={0};
  ioctl(device->fd,EVIOCGID,&id);
  desc->vid=id.vendor;
  desc->pid=id.product;
  desc->version=id.version;
  
  INPUT->namestorage[0]=0;
  ioctl(device->fd,EVIOCGNAME(sizeof(INPUT->namestorage)),INPUT->namestorage);
  INPUT->namestorage[sizeof(INPUT->namestorage)-1]=0;
  desc->name=INPUT->namestorage;
  desc->namec=0;
  while (desc->name[desc->namec]) {
    if ((desc->name[desc->namec]<0x20)||(desc->name[desc->namec]>0x7e)) {
      INPUT->namestorage[desc->namec]='?';
    }
    desc->namec++;
  }
  while (desc->namec&&((unsigned char)desc->name[desc->namec-1]<=0x20)) desc->namec--;
  while (desc->namec&&((unsigned char)desc->name[0]<=0x20)) { desc->namec--; desc->name++; }
  
  return 0;
}

/* Enumerate absolute axes -- these come with some extra data.
 */
 
static int rb_evdev_device_enumerate_abs(
  struct rb_evdev_device *device,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
) {
  #define bufsize (ABS_CNT+7)>>3
  uint8_t bits[bufsize]={0};
  int bytecount=ioctl(device->fd,EVIOCGBIT(EV_ABS,sizeof(bits)),bits);
  int major=0;
  for (;major<bytecount;major++) {
    if (!bits[major]) continue;
    int minor=0,mask=0x01;
    for (;minor<8;minor++,mask<<=1) {
      if (bits[major]&mask) {
        int code=(major<<3)|minor;
        struct input_absinfo ai={0};
        ioctl(device->fd,EVIOCGABS(code),&ai);
        uint32_t hidusage=rb_evdev_guess_hidusage(EV_ABS,code);
        int err=cb((EV_ABS<<16)|code,hidusage,ai.value,ai.minimum,ai.maximum,userdata);
        if (err) return err;
      }
    }
  }
  #undef bufsize
  return 0;
}

/* Enumerate any non-EV_ABS event type.
 */
 
static int rb_evdev_device_enumerate_code(
  struct rb_evdev_device *device,
  uint8_t type,int lo,int hi,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
) {
  // There's way more KEY events than any other; use that for buffer size.
  #define bufsize (KEY_CNT+7)>>3
  uint8_t bits[bufsize]={0};
  int bytecount=ioctl(device->fd,EVIOCGBIT(type,sizeof(bits)),bits);
  int major=0;
  for (;major<bytecount;major++) {
    if (!bits[major]) continue;
    int minor=0,mask=0x01;
    for (;minor<8;minor++,mask<<=1) {
      if (bits[major]&mask) {
        int code=(major<<3)|minor;
        uint32_t hidusage=rb_evdev_guess_hidusage(type,code);
        int err=cb((type<<16)|code,hidusage,0,lo,hi,userdata);
        if (err) return err;
      }
    }
  }
  #undef bufsize
  return 0;
}

/* Enumerate buttons.
 */

int rb_evdev_device_enumerate(
  struct rb_evdev_device *device,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
) {
  int err;
  if (err=rb_evdev_device_enumerate_abs(device,cb,userdata)) return err;
  if (err=rb_evdev_device_enumerate_code(device,EV_KEY,0,2,cb,userdata)) return err;
  if (err=rb_evdev_device_enumerate_code(device,EV_REL,-32768,32767,cb,userdata)) return err;
  return 0;
}
