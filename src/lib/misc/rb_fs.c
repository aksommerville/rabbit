#include "rabbit/rb_internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef O_BINARY
  // O Windows...
  #define O_BINARY 0
#endif

/* Read file with seeking.
 */
 
int rb_file_read(void *dstpp,const char *path) {
  if (!path||!path[0]) return -1;
  int fd=open(path,O_RDONLY|O_BINARY);
  if (fd<0) return -1;
  off_t flen=lseek(fd,0,SEEK_END);
  if ((flen<0)||(flen>INT_MAX)||lseek(fd,0,SEEK_SET)) {
    close(fd);
    return -1;
  }
  char *dst=malloc(flen);
  if (!dst) {
    close(fd);
    return -1;
  }
  int dstc=0;
  while (dstc<flen) {
    int err=read(fd,dst+dstc,flen-dstc);
    if (err<=0) {
      close(fd);
      free(dst);
      return -1;
    }
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Read file without seeking.
 */
 
// Ensure we give up on infinite streams at some point.
// I'm saying 32 MB for no particular reason.
#define RB_PIPE_READ_LIMIT (32*1024*1024)

int rb_file_read_pipesafe(void *dstpp,const char *path) {
  if (!path||!path[0]) return -1;
  int fd=open(path,O_RDONLY|O_BINARY);
  if (fd<0) return -1;
  int dstc=0,dsta=4096;
  char *dst=malloc(dsta);
  if (!dst) {
    close(fd);
    return -1;
  }
  while (1) {
    if (dstc>RB_PIPE_READ_LIMIT) {
      close(fd);
      free(dst);
      return -1;
    }
    if (dstc>=dsta) {
      dsta+=4096;
      void *nv=realloc(dst,dsta);
      if (!nv) {
        close(fd);
        free(dst);
        return -1;
      }
      dst=nv;
    }
    int err=read(fd,dst+dstc,dsta-dstc);
    if (err<0) {
      close(fd);
      free(dst);
      return -1;
    }
    if (!err) break;
    dstc+=err;
  }
  close(fd);
  *(void**)dstpp=dst;
  return dstc;
}

/* Write file.
 */

int rb_file_write(const char *path,const void *src,int srcc) {
  if (!path||!path[0]||(srcc<0)||(srcc&&!src)) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
  if (fd<0) return -1;
  int srcp=0;
  while (srcp<srcc) {
    int err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}

/* Walk directory.
 */

int rb_dir_read(
  const char *path,
  int (*cb_file)(const char *path,int pathc,const char *base,int basec,char type,void *userdata),
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
  
    const char *base=de->d_name;
    int basec=0;
    while (base[basec]) basec++;
    if (!basec) continue;
    if ((basec==1)&&(base[0]=='.')) continue;
    if ((basec==2)&&(base[0]=='.')&&(base[1]=='.')) continue;
    
    if (subpathc+basec>=sizeof(subpath)) {
      closedir(dir);
      return -1;
    }
    memcpy(subpath+subpathc,base,basec+1);
    
    char type=0;
    #if 1 // TODO enumerate systems that don't have d_type (beware MacOS has it but doesn't say so)
      switch (de->d_type) {
        case DT_REG: type='f'; break;
        case DT_DIR: type='d'; break;
        case DT_LNK: type='l'; break;
        case DT_SOCK: type='s'; break;
        default: type='?'; break;
      }
    #endif
    
    int err=cb_file(subpath,subpathc+basec,base,basec,type,userdata);
    if (err) {
      closedir(dir);
      return err;
    }
  }
  closedir(dir);
  return 0;
}

/* File type via stat.
 */

char rb_file_get_type(const char *path) {
  if (!path||!path[0]) return 0;
  struct stat st={0};
  if (stat(path,&st)<0) return 0;
  if (S_ISREG(st.st_mode)) return 'f';
  if (S_ISDIR(st.st_mode)) return 'd';
  if (S_ISLNK(st.st_mode)) return 'l'; // shouldn't happen; this is plain 'stat'
  if (S_ISSOCK(st.st_mode)) return 's';
  return '?';
}

/* Recursive mkdir from a leaf path.
 */
 
int rb_mkdir_for_file(const char *path) {
  if (!path) return 0;
  int pathc=0,slashp=-1;
  while (path[pathc]) {
    if (path[pathc]=='/') slashp=pathc;
    pathc++;
  }
  if (slashp<0) return 0;
  char subpath[1024];
  if (slashp>=sizeof(subpath)) return -1;
  memcpy(subpath,path,slashp);
  subpath[slashp]=0;
  
  if (mkdir(subpath,0777)>=0) return 0;
  if (errno!=ENOENT) return -1;
  if (rb_mkdir_for_file(subpath)<0) return -1;
  if (mkdir(subpath,0777)>=0) return 0;
  return -1;
}
