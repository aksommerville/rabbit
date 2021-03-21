#include "rb_evdev_internal.h"

/* Initialize inotify.
 */
int rb_evdev_set_root(struct rb_input *input,const char *path,int pathc) {
  if (!path) return -1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (!pathc||(path[0]!='/')||(path[pathc-1]!='/')) return -1;
  
  char *nv=malloc(pathc+1);
  if (!nv) return -1;
  memcpy(nv,path,pathc);
  nv[pathc]=0;
  if (INPUT->root) free(INPUT->root);
  INPUT->root=nv;
  INPUT->rootc=pathc;
  
  if (INPUT->fd>=0) {
    close(INPUT->fd);
  }
  if ((INPUT->fd=inotify_init())<0) return -1;
  
  if (inotify_add_watch(INPUT->fd,INPUT->root,IN_CREATE|IN_ATTRIB)<0) return -1;
  
  INPUT->refresh=1;
  
  return 0;
}

/* Drop any device which has run out of funk.
 */
 
int rb_evdev_drop_defunct_devices(struct rb_input *input) {
  int i=INPUT->devicec;
  struct rb_evdev_device *device=INPUT->devicev+i-1;
  for (;i-->0;device--) {
    if (device->fd<0) {
      rb_evdev_device_cleanup(device);
      INPUT->devicec--;
      memmove(device,device+1,sizeof(struct rb_evdev_device)*(INPUT->devicec-i));
    }
  }
  return 0;
}

/* Rebuild pollfdv.
 */

int rb_evdev_rebuild_pollfdv(struct rb_input *input) {

  int c=INPUT->devicec;
  if (INPUT->fd>=0) c++;
  if (c>INPUT->pollfda) {
    int na=(c+8)&~7;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(INPUT->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    INPUT->pollfdv=nv;
    INPUT->pollfda=na;
  }
  
  struct pollfd *pollfd=INPUT->pollfdv;
  if (INPUT->fd>=0) {
    pollfd->fd=INPUT->fd;
    pollfd->events=POLLIN|POLLHUP|POLLERR;
    pollfd->revents=0;
    pollfd++;
  }
  struct rb_evdev_device *device=INPUT->devicev;
  int i=INPUT->devicec;
  for (;i-->0;device++,pollfd++) {
    pollfd->fd=device->fd;
    pollfd->events=POLLIN|POLLHUP|POLLERR;
    pollfd->revents=0;
  }
  
  return c;
}

/* Notify of a file discovered in the root directory.
 * No error if we fail to open, or choose not to, or whatever.
 */
 
static int rb_evdev_consider_file(struct rb_input *input,const char *base,int basec) {
  
  // First things first, make sense of its name, check if we already have it.
  if ((basec<6)||memcmp(base,"event",5)) return 0;
  int evdevid=0,p=5;
  for (;p<basec;p++) {
    int digit=base[p]-'0';
    if ((digit<0)||(digit>9)) return 0;
    evdevid*=10;
    evdevid+=digit;
  }
  struct rb_evdev_device *device=rb_evdev_device_by_evdevid(input,evdevid);
  if (device) return 0;
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s%.*s",INPUT->rootc,INPUT->root,basec,base);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0; // the hell?
  
  // Open it and confirm that it responds to the evdev version ioctl.
  // Failure here, no big deal.
  // eg, it is normal and good in desktop Linux for the keyboard device to be unreadable.
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  int evdev_version=0;
  if (ioctl(fd,EVIOCGVERSION,&evdev_version)<0) {
    close(fd);
    return 0;
  }
  if (evdev_version!=EV_VERSION) {
    /* If we're going to log this, make sure we only do it once per driver...
    fprintf(stderr,
      "You are using evdev version 0x%08x but we are compiled against 0x%08x. Probably not a big deal.\n",
      evdev_version,EV_VERSION
    );
    /**/
  }
  
  // Attempt to grab -- prevents the Pi's keyboard from talking to our parent terminal too.
  int grab=1;
  if (ioctl(fd,EVIOCGRAB,&grab)<0) {
    // meh whatever
  }
  
  // Beyond this point, errors are real.
  if (rb_evdev_add_device(input,fd,evdevid)<0) {
    close(fd);
    return -1;
  }
  
  return 0;
}

/* Read inotify and maybe connect new devices.
 */
 
int rb_evdev_update_inotify(struct rb_input *input) {
  char buf[1024];
  int bufc=read(INPUT->fd,buf,sizeof(buf));
  if (bufc<=0) {
    fprintf(stderr,"%.*s: inotify closed. New input devices will not be detected.\n",INPUT->rootc,INPUT->root);
    close(INPUT->fd);
    INPUT->fd=-1;
    return 0;
  }
  int bufp=0;
  while (bufp<=bufc-sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    bufp+=event->len;
    const char *base=event->name;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    if (rb_evdev_consider_file(input,base,basec)<0) return -1;
  }
  return 0;
}

/* Scan the root directory.
 */

int rb_evdev_scan(struct rb_input *input) {
  DIR *dir=opendir(INPUT->root);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if (rb_evdev_consider_file(input,base,basec)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Deliver a device event to the delegate.
 */
 
static int rb_evdev_event(
  struct rb_input *input,
  int devid,
  const struct input_event *event
) {
  switch (event->type) {
    case EV_SYN: return 0; // don't care
    case EV_MSC: {
        // MSC_SCAN reports HID usage as events come in.
        // That's too late for us, but maybe somehow we could use it?
      } break;
    case EV_KEY:
    case EV_ABS:
    case EV_REL: {
        if (input->delegate.cb_event) {
          int btnid=(event->type<<16)|event->code;
          return input->delegate.cb_event(input,devid,btnid,event->value);
        }
      } break;
    default: {
        //fprintf(stderr,"%s discard event %02x:%04x=%d\n",__func__,event->type,event->code,event->value);
      }
  }
  return 0;
}

/* Update a device.
 */

int rb_evdev_update_device(struct rb_input *input,int fd) {
  struct rb_evdev_device *device=rb_evdev_device_by_fd(input,fd);
  if (!device) return -1; // How did we poll it if we don't know the fd? Something is quite amiss!
  
  struct input_event eventv[16];
  int eventc=read(fd,eventv,sizeof(eventv));
  
  /* When reading fails, eg device unplugged, it's little complicated.
   * Remove the device from our list first, then notify the delegate.
   * Otherwise, they might try to disconnect it manually or something, and really fuck things up.
   */
  if (eventc<=0) {
    int devid=device->devid,err=0;
    rb_evdev_device_cleanup(device);
    INPUT->devicec--;
    memmove(device,device+1,sizeof(struct rb_evdev_device)*(INPUT->devicev+INPUT->devicec-device));
    if (input->delegate.cb_disconnect) {
      err=input->delegate.cb_disconnect(input,devid);
    }
    return err;
  }
  
  /* Failure to process an event does not invalidate this device.
   * Rather, it's a hard error that goes all the way back to the caller.
   */
  int devid=device->devid; // capture devid just in case the delegate disconnects it
  eventc/=sizeof(struct input_event);
  const struct input_event *event=eventv;
  for (;eventc-->0;event++) {
    if (rb_evdev_event(input,devid,event)<0) return -1;
  }
  
  return 0;
}
