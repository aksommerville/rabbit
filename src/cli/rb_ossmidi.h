/* rb_ossmidi.h
 * Simple interface to OSS MIDI-In (which is itself a gloriously simple interface to the MIDI bus).
 * ALSA and Pulse systems likely emulate this interface, mine does.
 * (doing things the ALSA way is quite a bit more involved, meh...)
 */
 
#ifndef RB_OSSMIDI_H
#define RB_OSSMIDI_H

struct rb_ossmidi {
  int fd;
  int refresh;
  char *root;
  int rootc;
  struct rb_ossmidi_device {
    int fd;
    char *path;
    int pathc;
  } *devicev;
  int devicec,devicea;
  void *pollfdv;
  int pollfdc,pollfda;
};

struct rb_ossmidi *rb_ossmidi_new(const char *path,int pathc);
void rb_ossmidi_del(struct rb_ossmidi *ossmidi);

int rb_ossmidi_update(
  struct rb_ossmidi *ossmidi,
  int (*cb_serial)(const void *src,int srcc,const char *path,void *userdata),
  void *userdata
);

#endif
