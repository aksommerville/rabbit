#include "rb_demo.h"
#include "rabbit/rb_fs.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth_event.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>

#define ENV(a,d,r) ( \
  RB_ENV_FLAG_PRESET| \
  RB_ENV_PRESET_ATTACK##a| \
  RB_ENV_PRESET_DECAY##d| \
  RB_ENV_PRESET_RELEASE##r| \
0)

static const uint8_t synth_config[]={

  0x00,66, // bright cartoon piano
    RB_SYNTH_NTID_instrument,
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,61, // nodes
      RB_SYNTH_NTID_env,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U8,0x01, // mode=set
        0x03,RB_SYNTH_FIELD_TYPE_SERIAL1,11, // content
          RB_ENV_FLAG_INIT_LEVEL|RB_ENV_FLAG_LEVEL_RANGE,
          0x00,0x01,0x00,
          0x20,
          0x08,0xff,
          0x40,0xc0,
          0xf0,0x80,
        0,
      RB_SYNTH_NTID_fm,
        0x01,0x00, // main
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ, // rate
        0x03,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x00,0x00, // mod0
        0x04,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x01,0x00,0x00, // mod1
        0x05,0x01, // range
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_SERIAL1,13, // content
          RB_ENV_FLAG_CURVE|RB_ENV_FLAG_LEVEL_RANGE,
          0x00,0x00,0x10,
          0x04,0xff,0xc0,
          0x0c,0x50,0xc0,
          0x80,0x00,0x60,
        0x00,

  0x34,77, // ok electric guitar
    RB_SYNTH_NTID_instrument,
    0x01,0x00, // main
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,70, // nodes
      RB_SYNTH_NTID_fm,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ, // rate
        0x03,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x00,0x00, // mod0
        0x04,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x00,0x80,0x00, // mod1
        0x05,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x01,0x00,0x00, // range
        0x00,
      RB_SYNTH_NTID_mlt,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_add,
        0x01,0x01, // main
        0x02,RB_SYNTH_FIELD_TYPE_U0_8,0x80, // arg
        0x00,
      RB_SYNTH_NTID_osc,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x04,0x01, // phase
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0x40, // level
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main
        0x03,RB_SYNTH_FIELD_TYPE_U8,ENV(0,3,7),
        0x00,
      RB_SYNTH_NTID_gain,
        0x01,0x00, // main
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x05,0x00,0x00,
        0x03,RB_SYNTH_FIELD_TYPE_U0_8,0x10,
        0x00,

  0x35,77, // wacky brass
    RB_SYNTH_NTID_instrument,
    0x01,0x00, // main=buffer[0]
    0x02,RB_SYNTH_FIELD_TYPE_SERIAL2,0,70, // nodes
      RB_SYNTH_NTID_osc,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ,
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0xff, // level
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_U8,0x00, // mode=mlt
        0x03,RB_SYNTH_FIELD_TYPE_U8,
          RB_ENV_FLAG_PRESET
          |RB_ENV_PRESET_ATTACK1
          |RB_ENV_PRESET_DECAY1
          |RB_ENV_PRESET_RELEASE4,
        0x00,
        
      RB_SYNTH_NTID_osc,
        0x01,0x01, // main=buffer[1]
        0x02,RB_SYNTH_FIELD_TYPE_NOTEHZ,
        0x03,RB_SYNTH_FIELD_TYPE_U8,RB_OSC_SHAPE_SINE,
        0x05,RB_SYNTH_FIELD_TYPE_U0_8,0xff, // level
        0x00,
      RB_SYNTH_NTID_mlt,
        0x01,0x01, // main=buffer[1]
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x08,0x00,0x00,
        0x00,
      RB_SYNTH_NTID_env,
        0x01,0x01, // main=buffer[1]
        0x03,RB_SYNTH_FIELD_TYPE_U8,
          RB_ENV_FLAG_PRESET
          |RB_ENV_PRESET_ATTACK0
          |RB_ENV_PRESET_DECAY1
          |RB_ENV_PRESET_RELEASE5,
        0x00,
        
      RB_SYNTH_NTID_mlt,
        0x01,0x00, // main=buffer[0]
        0x02,0x01, // arg=buffer[1]
        0x00,
      RB_SYNTH_NTID_gain,
        0x01,0x00, // main=buffer[0]
        0x02,RB_SYNTH_FIELD_TYPE_S15_16,0x00,0x14,0x00,0x00, // gain
        0x03,RB_SYNTH_FIELD_TYPE_U0_8,0x10, // clip
        0x00,
        
  0x36,0x03,
    RB_SYNTH_NTID_beep,
    0x01,0x00, // main=buffer[0]
};

static int demo_multimidfile_play_file(const char *path) {

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
  
  struct rb_song *song=rb_song_from_midi(serial,serialc);
  free(serial);
  if (!song) {
    fprintf(stderr,"%s: Failed to decode MIDI file\n",path);
    return -1;
  }
  
  if (rb_audio_lock(rb_demo_audio)<0) {
    rb_song_del(song);
    return -1;
  }
  int err=rb_synth_play_song(rb_demo_synth,song,1);
  rb_audio_unlock(rb_demo_audio);
  rb_song_del(song);
  if (err<0) {
    fprintf(stderr,"rb_synth_play_song() failed\n");
    return -1;
  }
  
  fprintf(stderr,"%s: Playing MIDI file\n",path);
  return 0;
}

static char **mmf_pathv=0;
static int mmf_pathc=0;
static int mmf_patha=0;
static int mmf_pathp=0;

static int mmf_add_directory_cb(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  if ((pathc<4)||memcmp(path+pathc-4,".mid",4)) {
    fprintf(stderr,"%s: Ignoring, doesn't look like MIDI\n",path);
    return 0;
  }
  if (mmf_pathc>=mmf_patha) {
    int na=mmf_patha+32;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(mmf_pathv,sizeof(void*)*na);
    if (!nv) return -1;
    mmf_pathv=nv;
    mmf_patha=na;
  }
  char *nv=malloc(pathc+1);
  if (!nv) return -1;
  memcpy(nv,path,pathc);
  nv[pathc]=0;
  mmf_pathv[mmf_pathc++]=nv;
  return 0;
}

static int mmf_add_directory(const char *path) {
  return rb_dir_read(path,mmf_add_directory_cb,0);
}

static int demo_multimidfile_init() {
  
  if (rb_audio_lock(rb_demo_audio)<0) return -1;
  if (rb_synth_configure(rb_demo_synth,synth_config,sizeof(synth_config))<0) {
    rb_audio_unlock(rb_demo_audio);
    return -1;
  }
  rb_audio_unlock(rb_demo_audio);
  
  //demo_multimidfile_play_file("../chetyorska/src/music/mutopia/unpack/gnomus.mid");
  if (mmf_add_directory("../chetyorska/src/music/mutopia/unpack")<0) {
    fprintf(stderr,"failed to scan directory\n");
  }
  
  return 0;
}

static void demo_multimidfile_quit() {
  if (mmf_pathv) {
    while (mmf_pathc-->0) free(mmf_pathv[mmf_pathc]);
    free(mmf_pathv);
  }
  mmf_pathv=0;
  mmf_pathc=0;
  mmf_patha=0;
  mmf_pathp=0;
}

static int mmf_next() {
  if (mmf_pathp>=mmf_pathc) {
    fprintf(stderr,"end of song list\n");
    return 0;
  }
  const char *path=mmf_pathv[mmf_pathp++];
  if (demo_multimidfile_play_file(path)<0) {
    fprintf(stderr,"%s: Failed to play file\n",path);
  } else {
    //fprintf(stderr,"%s\n",path); // already logged
  }
  return 1;
}

static int demo_multimidfile_update() {

  if (!rb_demo_synth->song) {
    return mmf_next();
  }
  
  struct pollfd pollfd={.fd=STDIN_FILENO,.events=POLLIN|POLLERR|POLLHUP};
  if (poll(&pollfd,1,0)>0) {
    char tmp[128];
    int tmpc=read(STDIN_FILENO,tmp,sizeof(tmp));
    return mmf_next();
  }

  return 1;
}

RB_DEMO(multimidfile)
