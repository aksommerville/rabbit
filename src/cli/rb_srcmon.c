#include "rabbit/rb_internal.h"
#include "rb_srcmon.h"

#if RB_ARCH==RB_ARCH_linux

#include <dirent.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/inotify.h>

// In case someone tries to monitor "/" or something...
#define RB_SRCMON_WATCH_SANITY_LIMIT 128

static int rb_srcmon_discover_directories(struct rb_srcmon *srcmon);

/* Cleanup.
 */
 
static void rb_srcmon_watch_cleanup(struct rb_srcmon_watch *watch,int fd) {
  if ((watch->wd>=0)&&(fd>=0)) {
    inotify_rm_watch(fd,watch->wd);
  }
  if (watch->path) free(watch->path);
}
 
void rb_srcmon_del(struct rb_srcmon *srcmon) {
  if (!srcmon) return;
  
  if (srcmon->watchv) {
    while (srcmon->watchc>0) {
      srcmon->watchc--;
      rb_srcmon_watch_cleanup(srcmon->watchv+srcmon->watchc,srcmon->fd);
    }
    free(srcmon->watchv);
  }
  
  if (srcmon->fd>=0) close(srcmon->fd);
  if (srcmon->root) free(srcmon->root);
  if (srcmon->inbuf) free(srcmon->inbuf);
  
  free(srcmon);
}

/* New.
 */
 
struct rb_srcmon *rb_srcmon_new(const char *path,int pathc) {
  if (!path) return 0;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (!pathc) return 0;
  
  struct rb_srcmon *srcmon=calloc(1,sizeof(struct rb_srcmon));
  if (!srcmon) return 0;
  
  srcmon->fd=-1;
  srcmon->refresh=1;
  
  if (!(srcmon->root=malloc(pathc+2))) { // +2 for NUL terminator and possible '/'
    free(srcmon);
    return 0;
  }
  memcpy(srcmon->root,path,pathc);
  srcmon->rootc=pathc;
  if (srcmon->root[srcmon->rootc-1]!='/') {
    srcmon->root[srcmon->rootc++]='/';
  }
  srcmon->root[srcmon->rootc]=0;
  
  if ((srcmon->fd=inotify_init())<0) {
    rb_srcmon_del(srcmon);
    return 0;
  }
  
  if (rb_srcmon_discover_directories(srcmon)<0) {
    rb_srcmon_del(srcmon);
    return 0;
  }
  
  return srcmon;
}

/* Add a watch.
 */
 
static int rb_srcmon_add_watch(struct rb_srcmon *srcmon,const char *path) {
  if (!path||!path[0]) return -1;

  if (srcmon->watchc>=RB_SRCMON_WATCH_SANITY_LIMIT) {
    fprintf(stderr,"Count of source directories exceeded sanity limit of %d.\n",RB_SRCMON_WATCH_SANITY_LIMIT);
    return -1;
  }
  
  if (srcmon->watchc>=srcmon->watcha) {
    int na=srcmon->watcha+8;
    if (na>INT_MAX/sizeof(struct rb_srcmon_watch)) return -1;
    void *nv=realloc(srcmon->watchv,sizeof(struct rb_srcmon_watch)*na);
    if (!nv) return -1;
    srcmon->watchv=nv;
    srcmon->watcha=na;
  }
  
  struct rb_srcmon_watch *watch=srcmon->watchv+srcmon->watchc++;
  memset(watch,0,sizeof(struct rb_srcmon_watch));
  watch->wd=-1;
  
  int pathc=0;
  while (path[pathc]) pathc++;
  if (!(watch->path=malloc(pathc+2))) {
    srcmon->watchc--;
    return -1;
  }
  memcpy(watch->path,path,pathc);
  watch->pathc=pathc;
  if (watch->path[watch->pathc-1]!='/') watch->path[watch->pathc++]='/';
  watch->path[watch->pathc]=0;
  
  if ((watch->wd=inotify_add_watch(
    srcmon->fd,watch->path,IN_MODIFY|IN_MOVED_TO
  ))<0) {
    srcmon->watchc--;
    rb_srcmon_watch_cleanup(watch,srcmon->fd);
    return -1;
  }
  
  return 0;
}

/* Add a watch for this directory, then scan it and re-enter for sub-directories.
 * Provide the absolute* path, including srcmon->root.
 * [*] or at least as absolute as srcmon->root is.
 */
 
static int rb_srcmon_add_watch_and_scan(struct rb_srcmon *srcmon,const char *path) {

  char subpath[1024];
  int subpathc=0;
  while (path[subpathc]) subpathc++;
  if (subpathc>=sizeof(subpath)) return -1;
  memcpy(subpath,path,subpathc);
  if (!subpathc||(subpath[subpathc-1]!='/')) subpath[subpathc++]='/';

  DIR *dir=opendir(path);
  if (!dir) return -1;
  
  if (rb_srcmon_add_watch(srcmon,path)<0) {
    closedir(dir);
    return -1;
  }
  
  struct dirent *de;
  while (de=readdir(dir)) {
    
    if (de->d_name[0]=='.') continue;
    
    //TODO Do we need to support systems with no d_type?
    if (de->d_type!=DT_DIR) continue;
    
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if (subpathc>=sizeof(subpath)-basec) {
      closedir(dir);
      return -1;
    }
    memcpy(subpath+subpathc,base,basec+1);
    
    if (rb_srcmon_add_watch_and_scan(srcmon,subpath)<0) {
      closedir(dir);
      return -1;
    }
  }
  closedir(dir);
  return 0;
}

/* Scan recursively for directories and add a watch for everything we find (including the root).
 */
 
static int rb_srcmon_discover_directories(struct rb_srcmon *srcmon) {
  
  // Drop existing watches. Not currently possible to have any, but this makes it safe to call whenever.
  while (srcmon->watchc>0) {
    srcmon->watchc--;
    rb_srcmon_watch_cleanup(srcmon->watchv+srcmon->watchc,srcmon->fd);
  }
  
  // Begin recursion.
  if (rb_srcmon_add_watch_and_scan(srcmon,srcmon->root)<0) return -1;
  
  return 0;
}

/* Scan one directory without recursion and report each regular file.
 */
 
static int rb_srcmon_scan_path(
  struct rb_srcmon *srcmon,
  const char *path,
  int (*cb_changed)(const char *base,int basec,const char *path,int pathc,void *userdata),
  void *userdata
) {

  if (!path||!path[0]) return -1;
  char subpath[1024];
  int subpathc=0;
  while (path[subpathc]) subpathc++;
  if (subpathc>=sizeof(subpath)) return -1;
  memcpy(subpath,path,subpathc);
  if (subpath[subpathc-1]!='/') subpath[subpathc++]='/';

  DIR *dir=opendir(path);
  if (!dir) return -1;
  struct dirent *de;
  while (de=readdir(dir)) {
  
    if (de->d_name[0]=='.') continue;
    if (de->d_type!=DT_REG) continue;
    
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if (subpathc>=sizeof(subpath)-basec) {
      closedir(dir);
      return -1;
    }
    memcpy(subpath+subpathc,base,basec+1);
    
    int err=cb_changed(subpath,subpathc+basec,base,basec,userdata);
    if (err) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* Scan each watched directory and report all regular files.
 */
 
static int rb_srcmon_scan(
  struct rb_srcmon *srcmon,
  int (*cb_changed)(const char *base,int basec,const char *path,int pathc,void *userdata),
  void *userdata
) {
  struct rb_srcmon_watch *watch=srcmon->watchv;
  int i=srcmon->watchc;
  for (;i-->0;watch++) {
    int err=rb_srcmon_scan_path(srcmon,watch->path,cb_changed,userdata);
    if (err) return err;
  }
  return 0;
}

/* Refill the inotify input buffer.
 */
 
static int rb_srcmon_read_inotify(struct rb_srcmon *srcmon) {
  
  if (srcmon->inbufp+srcmon->inbufc>=srcmon->inbufa) {
    if (srcmon->inbufp) {
      memmove(srcmon->inbuf,srcmon->inbuf+srcmon->inbufp,srcmon->inbufc);
      srcmon->inbufp=0;
    } else {
      int na=(srcmon->inbufp+srcmon->inbufc+2048)&~1023;
      void *nv=realloc(srcmon->inbuf,na);
      if (!nv) return -1;
      srcmon->inbuf=nv;
      srcmon->inbufa=na;
    }
  }
  
  int err=read(srcmon->fd,srcmon->inbuf+srcmon->inbufp+srcmon->inbufc,srcmon->inbufa-srcmon->inbufc-srcmon->inbufp);
  if (err<=0) {
    fprintf(stderr,"%.*s: Error reading from inotify\n",srcmon->rootc,srcmon->root);
    while (srcmon->watchc>0) {
      srcmon->watchc--;
      rb_srcmon_watch_cleanup(srcmon->watchv+srcmon->watchc,srcmon->fd);
    }
    close(srcmon->fd);
    srcmon->fd=-1;
    return -1;
  }
  srcmon->inbufc+=err;
  
  return 0;
}

/* Return watch struct for a given wd.
 */
 
static struct rb_srcmon_watch *rb_srcmon_find_watch(struct rb_srcmon *srcmon,int wd) {
  struct rb_srcmon_watch *watch=srcmon->watchv;
  int i=srcmon->watchc;
  for (;i-->0;watch++) {
    if (watch->wd==wd) return watch;
  }
  return 0;
}

/* Pop events from the inotify input buffer.
 */
 
static int rb_srcmon_report_inotify_events(
  struct rb_srcmon *srcmon,
  int (*cb_changed)(const char *base,int basec,const char *path,int pathc,void *userdata),
  void *userdata
) {
  char subpath[1024];
  int subpathc=0; // lazy init
  while (srcmon->inbufc>=sizeof(struct inotify_event)) {
  
    struct inotify_event *event=(struct inotify_event*)(srcmon->inbuf+srcmon->inbufp);
    int eventsize=sizeof(struct inotify_event)+event->len;
    if (srcmon->inbufc<eventsize) break;
    if (srcmon->inbufc-=eventsize) srcmon->inbufp+=eventsize;
    else srcmon->inbufp=0;
    
    struct rb_srcmon_watch *watch=rb_srcmon_find_watch(srcmon,event->wd);
    if (!watch) continue;
    
    const char *base=event->name;
    int basec=0;
    while ((basec<event->len)&&base[basec]) basec++;
    if (!basec||(base[0]=='.')) continue;
    
    if (!subpathc) {
      if (watch->pathc>=sizeof(subpath)) return -1;
      memcpy(subpath,watch->path,watch->pathc);
      subpathc=watch->pathc;
    }
    if (subpathc>=sizeof(subpath)-basec) return -1;
    memcpy(subpath+subpathc,base,basec+1);
    
    int err=cb_changed(subpath,subpathc+basec,base,basec,userdata);
    if (err) return err;
  }
  return 0;
}

/* Update.
 */
 
int rb_srcmon_update(
  struct rb_srcmon *srcmon,
  int (*cb_changed)(const char *base,int basec,const char *path,int pathc,void *userdata),
  void *userdata
) {
  if (!srcmon) return 0;
  if (srcmon->fd<0) return 0; // defunct, whatever
  
  if (srcmon->refresh) {
    int err=rb_srcmon_scan(srcmon,cb_changed,userdata);
    srcmon->refresh=0;
    if (err) return err;
  }
  
  struct pollfd pollfd={.fd=srcmon->fd,.events=POLLIN|POLLHUP|POLLERR};
  if (poll(&pollfd,1,0)<=0) {
    // Nothing to read, but still check the buffer in case our owner aborted the last update.
    if (srcmon->inbufc) return rb_srcmon_report_inotify_events(srcmon,cb_changed,userdata);
    return 0;
  }
  
  if (rb_srcmon_read_inotify(srcmon)<0) return -1;
  return rb_srcmon_report_inotify_events(srcmon,cb_changed,userdata);
}

#else /* not linux, create stubs */

struct rb_srcmon *rb_srcmon_new(const char *path,int pathc) {
  return calloc(1,sizeof(struct rb_srcmon));
}

void rb_srcmon_del(struct rb_srcmon *srcmon) {
  if (srcmon) free(srcmon);
}

int rb_srcmon_update(
  struct rb_srcmon *srcmon,
  int (*cb_changed)(const char *base,int basec,const char *path,int pathc,void *userdata),
  void *userdata
) {
  return 0;
}

#endif
