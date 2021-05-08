#include "rabbit/rb_internal.h"
#include "rabbit/rb_fs.h"
#include "rb_ossmidi.h"

#if RB_ARCH==RB_ARCH_linux

#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/inotify.h>

/* Cleanup.
 */
 
static void rb_ossmidi_device_cleanup(struct rb_ossmidi_device *device) {
  if (device->fd>=0) close(device->fd);
  if (device->path) free(device->path);
}

void rb_ossmidi_del(struct rb_ossmidi *ossmidi) {
  if (!ossmidi) return;
  if (ossmidi->root) free(ossmidi->root);
  if (ossmidi->fd>=0) close(ossmidi->fd);
  if (ossmidi->devicev) {
    while (ossmidi->devicec-->0) {
      rb_ossmidi_device_cleanup(ossmidi->devicev+ossmidi->devicec);
    }
    free(ossmidi->devicev);
  }
  if (ossmidi->pollfdv) free(ossmidi->pollfdv);
  free(ossmidi);
}

/* New.
 */

struct rb_ossmidi *rb_ossmidi_new(const char *path,int pathc) {
  if (!path) return 0;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if ((pathc<1)||(path[pathc-1]!='/')) return 0;
  
  struct rb_ossmidi *ossmidi=calloc(1,sizeof(struct rb_ossmidi));
  if (!ossmidi) return 0;
  
  if ((ossmidi->fd=inotify_init())<0) {
    rb_ossmidi_del(ossmidi);
    return 0;
  }
  
  if (!(ossmidi->root=malloc(pathc+1))) {
    rb_ossmidi_del(ossmidi);
    return 0;
  }
  memcpy(ossmidi->root,path,pathc);
  ossmidi->root[pathc]=0;
  ossmidi->rootc=pathc;
  
  ossmidi->refresh=1;
  
  if (inotify_add_watch(ossmidi->fd,ossmidi->root,IN_CREATE|IN_ATTRIB)<0) {
    rb_ossmidi_del(ossmidi);
    return 0;
  }
  
  return ossmidi;
}

/* Add a device if we don't already have it.
 */
 
static int rb_ossmidi_add_device(
  struct rb_ossmidi *ossmidi,
  const char *path,int pathc
) {
  struct rb_ossmidi_device *device=ossmidi->devicev;
  int i=ossmidi->devicec;
  for (;i-->0;device++) {
    if (device->pathc!=pathc) continue;
    if (memcmp(device->path,path,pathc)) continue;
    return 0;
  }
  
  if (ossmidi->devicec>=ossmidi->devicea) {
    int na=ossmidi->devicea+8;
    if (na>INT_MAX/sizeof(struct rb_ossmidi_device)) return -1;
    void *nv=realloc(ossmidi->devicev,sizeof(struct rb_ossmidi_device)*na);
    if (!nv) return -1;
    ossmidi->devicev=nv;
    ossmidi->devicea=na;
  }
  
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0; // Don't call it an error, device might not be readable yet or whatever.
  
  device=ossmidi->devicev+ossmidi->devicec++;
  device->fd=fd;
  if (!(device->path=malloc(pathc+1))) {
    close(fd);
    ossmidi->devicec--;
    return -1;
  }
  memcpy(device->path,path,pathc);
  device->path[pathc]=0;
  device->pathc=pathc;
  
  fprintf(stderr,"%.*s: Added ossmidi device\n",pathc,path);
  
  return 0;
}

/* Scan for devices.
 */
 
static int rb_ossmidi_scan_cb(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_ossmidi *ossmidi=userdata;
  if (type&&(type!='?')) return 0; // character devices are "other" to Rabbit.
  if (basec<5) return 0;
  if (memcmp(base,"midi",4)) return 0;
  int i=4; for (;i<basec;i++) {
    if ((base[i]<'0')||(base[i]>'9')) return 0;
  }
  if (rb_ossmidi_add_device(ossmidi,path,pathc)<0) return -1;
  return 0;
}
 
static int rb_ossmidi_scan(struct rb_ossmidi *ossmidi) {
  return rb_dir_read(ossmidi->root,rb_ossmidi_scan_cb,ossmidi);
}

/* Add a pollfd to the list.
 */
 
static int rb_ossmidi_pollfd_add(struct rb_ossmidi *ossmidi,int fd) {
  if (ossmidi->pollfdc>=ossmidi->pollfda) {
    int na=ossmidi->pollfda+4;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(ossmidi->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    ossmidi->pollfdv=nv;
    ossmidi->pollfda=na;
  }
  struct pollfd *pollfd=((struct pollfd*)ossmidi->pollfdv)+ossmidi->pollfdc++;
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  pollfd->revents=0;
  return 0;
}

/* Read from inotify.
 */
 
static int rb_ossmidi_update_inotify(struct rb_ossmidi *ossmidi) {
  char buf[1024];
  int bufc=read(ossmidi->fd,buf,sizeof(buf));
  if (bufc<=0) {
    fprintf(stderr,"%.*s: Failed to read from inotify. Will not detect any new MIDI devices.\n",ossmidi->rootc,ossmidi->root);
    close(ossmidi->fd);
    ossmidi->fd=-1;
    return 0;
  }
  char subpath[1024];
  int subpathc=0;
  int bufp=0;
  while (bufp<=bufc-sizeof(struct inotify_event)) {
    struct inotify_event *event=(struct inotify_event*)(buf+bufp);
    bufp+=sizeof(struct inotify_event);
    if (bufp>bufc-event->len) break;
    bufp+=event->len;
    
    const char *base=event->name;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    if (basec<5) continue;
    if (memcmp(base,"midi",4)) continue;
    int i=4,ok=1; for (;i<basec;i++) {
      if ((base[i]<'0')||(base[i]>'9')) {
        ok=0;
        break;
      }
    }
    if (!ok) continue;
    
    if (!subpathc) {
      if (ossmidi->rootc>sizeof(subpath)) return -1;
      memcpy(subpath,ossmidi->root,ossmidi->rootc);
      subpathc=ossmidi->rootc;
    }
    if (subpathc+basec>=sizeof(subpath)) return -1;
    memcpy(subpath+subpathc,base,basec+1);
  
    if (rb_ossmidi_add_device(ossmidi,subpath,subpathc+basec)<0) return -1;
  }
  return 0;
}

/* Drop device by fd.
 */
 
static void rb_ossmidi_drop_device(struct rb_ossmidi *ossmidi,int fd) {
  int i=ossmidi->devicec;
  while (i-->0) {
    struct rb_ossmidi_device *device=ossmidi->devicev+i;
    if (device->fd==fd) {
      rb_ossmidi_device_cleanup(device);
      ossmidi->devicec--;
      memmove(device,device+1,sizeof(struct rb_ossmidi_device)*(ossmidi->devicec-i));
      return;
    }
  }
}

/* Update device.
 */

int rb_ossmidi_update_device(
  struct rb_ossmidi *ossmidi,
  int fd,
  int (*cb_serial)(const void *src,int srcc,const char *path,void *userdata),
  void *userdata
) {
  const char *path="?";
  struct rb_ossmidi_device *device=ossmidi->devicev;
  int i=ossmidi->devicec;
  for (;i-->0;device++) {
    if (device->fd==fd) {
      path=device->path;
      break;
    }
  }
  
  char buf[1024];
  int bufc=read(fd,buf,sizeof(buf));
  if (bufc<=0) {
    fprintf(stderr,"%s: Failed to read from MIDI device, closing.\n",path);
    rb_ossmidi_drop_device(ossmidi,fd);
    return 0;
  }
  
  return cb_serial(buf,bufc,path,userdata);
}

/* Update.
 */

int rb_ossmidi_update(
  struct rb_ossmidi *ossmidi,
  int (*cb_serial)(const void *src,int srcc,const char *path,void *userdata),
  void *userdata
) {
  if (!ossmidi) return 0;
  
  if (ossmidi->refresh) {
    ossmidi->refresh=0;
    if (rb_ossmidi_scan(ossmidi)<0) return -1;
  }
  
  ossmidi->pollfdc=0;
  if (ossmidi->fd>=0) {
    if (rb_ossmidi_pollfd_add(ossmidi,ossmidi->fd)<0) return -1;
  }
  int i=ossmidi->devicec;
  struct rb_ossmidi_device *device=ossmidi->devicev+i-1;
  for (;i-->0;device--) {
    if (device->fd<0) {
      rb_ossmidi_device_cleanup(device);
      ossmidi->devicec--;
      memmove(device,device+1,sizeof(struct rb_ossmidi_device)*(ossmidi->devicec-i));
    } else {
      if (rb_ossmidi_pollfd_add(ossmidi,device->fd)<0) return -1;
    }
  }
  if (!ossmidi->pollfdc) return 0;
  
  if (poll(ossmidi->pollfdv,ossmidi->pollfdc,0)<=0) return 0;
  
  struct pollfd *pollfd=ossmidi->pollfdv;
  for (i=ossmidi->pollfdc;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (pollfd->fd==ossmidi->fd) {
      if (rb_ossmidi_update_inotify(ossmidi)<0) return -1;
    } else {
      if (rb_ossmidi_update_device(ossmidi,pollfd->fd,cb_serial,userdata)<0) return -1;
    }
  }
  
  return 0;
}

#else

struct rb_ossmidi *rb_ossmidi_new(const char *path,int pathc) {
  return calloc(1,sizeof(struct rb_ossmidi));
}

void rb_ossmidi_del(struct rb_ossmidi *ossmidi) {
  if (ossmidi) free(ossmidi);
}

int rb_ossmidi_update(
  struct rb_ossmidi *ossmidi,
  int (*cb_serial)(const void *src,int srcc,const char *path,void *userdata),
  void *userdata
) {
  return 0;
}

#endif
