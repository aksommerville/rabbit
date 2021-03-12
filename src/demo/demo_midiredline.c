#define RB_DEMO_USE_VIDEO 0
#define RB_DEMO_USE_AUDIO 0

#include "rb_demo.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth_event.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#define FILE_PATH "src/demo/data/cleopha.mid"
//#define FILE_PATH "src/demo/data/4-crow-no-maestro.mid"
//#define FILE_PATH 0

#define RATE 44100
#define CHANC 1

static const uint8_t synth_config[]={
  0x00,0x03,
    RB_SYNTH_NTID_beep,
    0x01,0x00, // main=buffer[0]
  0x36,0x03,
    RB_SYNTH_NTID_beep,
    0x01,0x00, // main=buffer[0]
};
 
static double now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+tv.tv_usec/1000000.0f;
}

static void demo_midiredline_quit() {
}

static int demo_midiredline_play_file(struct rb_synth *synth,const char *path) {

  int fd=open(path,O_RDONLY);
  if (fd<0) {
    fprintf(stderr,"%s: open failed\n",path);
    return -1;
  }
  int seriala=16384;
  char *serial=malloc(seriala);
  if (!serial) {
    close(fd);
    return -1;
  }
  int serialc=0;
  while (1) {
    if (serialc>=seriala) {
      seriala<<=1;
      void *nv=realloc(serial,seriala);
      if (!nv) {
        close(fd);
        free(serial);
        return -1;
      }
      serial=nv;
    }
    int err=read(fd,serial+serialc,seriala-serialc);
    if (err<0) {
      close(fd);
      free(serial);
      fprintf(stderr,"%s: read failed\n",path);
      return -1;
    }
    if (!err) break;
    serialc+=err;
  }
  close(fd);
  
  struct rb_song *song=rb_song_new(serial,serialc,synth->rate);
  free(serial);
  if (!song) {
    fprintf(stderr,"%s: Failed to decode MIDI file\n",path);
    return -1;
  }
  
  int err=rb_synth_play_song(synth,song,1);
  rb_song_del(song);
  if (err<0) {
    fprintf(stderr,"rb_synth_play_song() failed\n");
    return -1;
  }
  synth->song->repeat=0;
  
  return 0;
}

static int demo_midiredline_init() {

  struct rb_synth *synth=rb_synth_new(RATE,CHANC);
  if (!synth) return -1;

  if (demo_midiredline_play_file(synth,FILE_PATH)<0) {
    fprintf(stderr,"%s: Failed to play MIDI file\n",FILE_PATH);
    return -1;
  }
  
  if (rb_synth_configure(synth,synth_config,sizeof(synth_config))<0) return -1;
  
  double starttime=now();
  
  int framec=0;
  int16_t buf[1024];
  while (synth->song&&(framec<7425025)) { // 7425024 does not segfault
    if (rb_synth_update(buf,1024,synth)<0) return -1;
    framec+=1024;
  }
  
  double endtime=now();
  double elapsed=endtime-starttime;
  double generated=(double)framec/(double)synth->rate;
  double score=elapsed/generated;
  fprintf(stderr,
    "%s: rate=%d chanc=%d elapsed=%.03fs generated=%.03fs score=%.06f (%.0fx)\n",
    FILE_PATH,synth->rate,synth->chanc,elapsed,generated,score,1.0/score
  );
  
  rb_synth_del(synth);
  
  return 0;
}

static int demo_midiredline_update() {
  return 0;
}

RB_DEMO(midiredline)
