/* rb_srcmon.h
 * Structured interface to inotify, for tracking input files.
 * We operate on directories recursively, but only directories that exist at construction.
 * (detecting directory changes via inotify I'm sure is possible, but why complicate it?)
 * We ignore anything with a leading dot name, directories and files alike.
 */
 
#ifndef RB_SRCMON_H
#define RB_SRCMON_H

struct rb_srcmon {
  int fd;
  int refresh;
  char *root;
  int rootc;
  struct rb_srcmon_watch {
    int wd;
    char *path; // full path including (root) and a '/' terminator
    int pathc;
  } *watchv;
  int watchc,watcha;
  char *inbuf;
  int inbufp,inbufc,inbufa;
};

/* Creating a srcmon scans its root directory and may fail due to I/O or Not Found.
 */
struct rb_srcmon *rb_srcmon_new(const char *path,int pathc);
void rb_srcmon_del(struct rb_srcmon *srcmon);

/* Poll inotify and call (cb_changed) for any file created or written.
 * First time you update, or if you set (srcmon->refresh), we read the directories and report every file.
 */
int rb_srcmon_update(
  struct rb_srcmon *srcmon,
  int (*cb_changed)(const char *base,int basec,const char *path,int pathc,void *userdata),
  void *userdata
);

#endif
