#include "rb_demo.h"
#include "rabbit/rb_synth_node.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>

#define OSS_PATH "/dev/midi1"

static const uint8_t synth_config[]={
  0x00,0x03,
    RB_SYNTH_NTID_beep,
    0x01,0x00, // main=buffer[0]
};

static int midiin_fd=-1;

static void demo_midiin_quit() {
  if (midiin_fd>=0) {
    close(midiin_fd);
    midiin_fd=-1;
  }
}

static int demo_midiin_init() {

  if ((midiin_fd=open(OSS_PATH,O_RDONLY))<0) {
    fprintf(stderr,"%s: Failed to open MIDI device -- does it exist?\n",OSS_PATH);
    return -1;
  }
  
  if (rb_audio_lock(rb_demo_audio)<0) return -1;
  if (rb_synth_configure(rb_demo_synth,synth_config,sizeof(synth_config))<0) {
    rb_audio_unlock(rb_demo_audio);
    return -1;
  }
  rb_audio_unlock(rb_demo_audio);
  
  return 0;
}

static int demo_midiin_update() {
  if (midiin_fd<0) return 0;
  struct pollfd pollfd={.fd=midiin_fd,.events=POLLIN|POLLERR|POLLHUP};
  if (poll(&pollfd,1,0)<=0) return 1;
  char buf[1024];
  int bufc=read(midiin_fd,buf,sizeof(buf));
  if (bufc<=0) {
    fprintf(stderr,"%s: Error reading.\n",OSS_PATH);
    return 0;
  }
  if (rb_audio_lock(rb_demo_audio)<0) return -1;
  if (rb_synth_events(rb_demo_synth,buf,bufc)<0) {
    fprintf(stderr,"rb_synth_events() failed with %d bytes input\n",bufc);
    rb_audio_unlock(rb_demo_audio);
    return -1;
  }
  rb_audio_unlock(rb_demo_audio);
  return 1;
}

RB_DEMO(midiin)
