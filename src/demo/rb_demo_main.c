#include "rb_demo.h"
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <math.h>

/* Globals.
 */
 
static const struct rb_demo *rb_demo=0;
static volatile int rb_sigc=0;
static volatile int rb_terminate=0;
struct rb_video *rb_demo_video=0;
struct rb_image *rb_demo_fb=0;
struct rb_image *rb_demo_override_fb=0;
struct rb_audio *rb_demo_audio=0;
struct rb_synth *rb_demo_synth=0;
int rb_demo_mousex=0;
int rb_demo_mousey=0;
static double rb_demo_starttime=0.0;
static int rb_demo_framec=0;
static double rb_demo_austarttime=0.0;
static double rb_demo_auendtime=0.0;
static int rb_demo_auframec=0;
static struct timespec starttime={0},cpustarttime={0};

/* Current real time.
 */
 
static double rb_demo_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+tv.tv_usec/1000000.0;
}

static double rb_demo_threadtime() {
  struct timespec tv={0};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+tv.tv_nsec/1000000000.0;
}

static void rb_demo_initialize_average_load() {
  clock_gettime(CLOCK_REALTIME,&starttime);
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&cpustarttime);
}

static double rb_demo_calculate_average_load() {
  struct timespec tvcpu={0},tvnow={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tvcpu);
  clock_gettime(CLOCK_REALTIME,&tvnow);
  double realend=(double)tvnow.tv_sec+tvnow.tv_nsec/1000000000.0;
  double realstart=(double)starttime.tv_sec+starttime.tv_nsec/1000000000.0;
  double cpuend=(double)tvcpu.tv_sec+tvcpu.tv_nsec/1000000000.0;
  double cpustart=(double)cpustarttime.tv_sec+cpustarttime.tv_nsec/1000000000.0;
  return (cpuend-cpustart)/(realend-realstart);
}

/* Signal handler.
 */
 
static void rb_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++rb_sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Cleanup.
 */
 
static void rb_demo_quit(int status) {

  double endtime=rb_demo_now();
  int audiorate=1;
  
  if (rb_demo_audio) {
    audiorate=rb_demo_audio->delegate.rate;
    rb_audio_del(rb_demo_audio);
    rb_demo_audio=0;
  }

  rb_demo->quit();
  
  if (rb_demo_video) {
    rb_video_del(rb_demo_video);
    rb_demo_video=0;
  }
  if (rb_demo_synth) {
    rb_synth_del(rb_demo_synth);
    rb_demo_synth=0;
  }
  
  rb_image_del(rb_demo_fb);
  rb_demo_fb=0;
  
  if (status) {
    fprintf(stderr,"%s: Terminate due to error.\n",rb_demo->name);
  } else {
    
    if (rb_demo_framec>0) {
      double elapsed=endtime-rb_demo_starttime;
      double rate=rb_demo_framec/elapsed;
      fprintf(stderr,
        "%s:VIDEO: %d frames in %.03fs, average %.03f Hz.\n",
        rb_demo->name,rb_demo_framec,elapsed,rate
      );
    }
    
    if (rb_demo_auframec>0) {
      double elapsed=rb_demo_auendtime-rb_demo_austarttime;
      double generated=(double)rb_demo_auframec/(double)audiorate;
      double score=elapsed/generated;
      fprintf(stderr,
        "%s:AUDIO: Generated %d frames (%.03fs) in %.03fs, score=%.06f (%.0fx)\n",
        rb_demo->name,rb_demo_auframec,generated,elapsed,score,1.0/score
      );
    }
    
    double average_load=rb_demo_calculate_average_load();
    fprintf(stderr,"Average CPU consumption (0..1): %.06f\n",average_load);
  }
}

/* Video manager callbacks.
 */
 
static int rb_demo_cb_close(struct rb_video *video) {
  rb_terminate=1;
  return 0;
}

static int rb_demo_cb_mmotion(struct rb_video *video,int x,int y) {
  rb_demo_mousex=x;
  rb_demo_mousey=y;
  return 0;
}

static int rb_demo_cb_mbutton(struct rb_video *video,int btnid,int value) {
  if (btnid==1) {
    if (rb_demo->cb_click) {
      return rb_demo->cb_click(rb_demo_mousex,rb_demo_mousey,value);
    }
  }
  return 0;
}

/* Audio callback. 
 */
 
static int rb_demo_cb_pcm_out(int16_t *v,int c,struct rb_audio *audio) {
  if (!rb_demo_auframec) {
    rb_demo_austarttime=rb_demo_threadtime();
  }
  if (rb_demo_synth) {
    if (rb_synth_update(v,c,rb_demo_synth)<0) return -1;
    if (rb_demo_synth->messagec) {
      fprintf(stderr,"Synth error: %.*s\n",rb_demo_synth->messagec,rb_demo_synth->message);
      rb_synth_clear_error(rb_demo_synth);
    }
  } else {
    memset(v,0,c<<1);
  }
  rb_demo_auendtime=rb_demo_threadtime();
  rb_demo_auframec+=c;
  return 0;
}

/* Keyboard event.
 */
 
static int rb_demo_cb_key(struct rb_video *video,int keycode,int value) {
  //fprintf(stderr,"%s %08x=%d\n",__func__,keycode,value);
  
  // A few things we'll do without being asked...
  if (value==1) switch (keycode) {
    case 0x00070009: rb_video_set_fullscreen(video,video->fullscreen?0:1); return 0; // F
    case 0x00070029: rb_terminate=1; return 0; // Escape
  }
  
  if (rb_demo->cb_key) return rb_demo->cb_key(keycode,value);
  
  return 0;
}

/* Initialize.
 */
 
static int rb_demo_init() {

  signal(SIGINT,rb_rcvsig);
  
  if (!(rb_demo_fb=rb_framebuffer_new())) return -1;

  if (rb_demo->use_video) {
    struct rb_video_delegate delegate={
      .title=rb_demo->name,
      .cb_close=rb_demo_cb_close,
      .cb_mmotion=rb_demo_cb_mmotion,
      .cb_mbutton=rb_demo_cb_mbutton,
      .cb_key=rb_demo_cb_key,
      .fullscreen=0,
    };
    if (!(rb_demo_video=rb_video_new(0,&delegate))) {
      fprintf(stderr,"Failed to initialize default video driver.\n");
      return -1;
    }
  }
  
  if (rb_demo->use_audio) {
    struct rb_audio_delegate delegate={
      .rate=44100,
      .chanc=1,
      .cb_pcm_out=rb_demo_cb_pcm_out,
    };
    if (!(rb_demo_audio=rb_audio_new(0,&delegate))) {
      fprintf(stderr,"Failed to initialize default audio driver.\n");
      return -1;
    }
    if (!(rb_demo_synth=rb_synth_new(rb_demo_audio->delegate.rate,rb_demo_audio->delegate.chanc))) {
      fprintf(stderr,"Failed to initialize synthesizer.\n");
      return -1;
    }
  }

  if (rb_demo->init()<0) {
    fprintf(stderr,"%s: init failed\n",rb_demo->name);
    return -1;
  }
  
  rb_demo_starttime=rb_demo_now();
  rb_demo_initialize_average_load();
  
  return 0;
}

/* Main loop.
 */
 
static int rb_demo_main() {
  while (!rb_sigc&&!rb_terminate) {
  
    int err=rb_demo->update();
    if (err<0) {
      fprintf(stderr,"%s: update failed\n",rb_demo->name);
      return -1;
    }
    if (!err) return 0;
    
    if (rb_demo_audio) {
      if (rb_audio_update(rb_demo_audio)<0) {
        fprintf(stderr,"Update audio driver '%s' failed\n",rb_demo_audio->type->name);
        return -1;
      }
    }
    
    if (rb_demo_video) {
      if (rb_video_update(rb_demo_video)<0) {
        fprintf(stderr,"Updating video driver '%s' failed\n",rb_demo_video->type->name);
        return -1;
      }
      struct rb_image *fb=rb_demo_fb;
      if (rb_demo_override_fb) fb=rb_demo_override_fb;
      if (rb_video_swap(rb_demo_video,fb)<0) {
        fprintf(stderr,"Video '%s': swap failed\n",rb_demo_video->type->name);
        return -1;
      }
    } else {
      //TODO Need a global timing regulator.
      usleep(10000);
    }

    rb_demo_framec++;
  }
  return 0;
}

/* Get demo by name.
 * Logs errors.
 */
 
#define _(_name) extern const struct rb_demo rb_demo_##_name;
RB_FOR_EACH_DEMO
#undef _
 
static const struct rb_demo *rb_get_demo_by_name(const char *name) {
  #define _(_name) if (!strcmp(rb_demo_##_name.name,name)) return &rb_demo_##_name;
  RB_FOR_EACH_DEMO
  #undef _
  fprintf(stderr,"Demo '%s' not found. Available:\n",name);
  #define _(_name) fprintf(stderr,"  %s%s\n",#_name,strcmp(#_name,name)?"":" [default]");
  RB_FOR_EACH_DEMO
  #undef _
  return 0;
}

/* Main entry point.
 */
 
int main(int argc,char **argv) {
  const char *name=RB_DEFAULT_DEMO_NAME;
  if (argc>2) {
    fprintf(stderr,"Usage: %s [NAME]\n",argv[0]);
    return 1;
  } else if (argc==2) {
    name=argv[1];
  }
  if (!(rb_demo=rb_get_demo_by_name(name))) return 1;
  fprintf(stderr,"%s: Starting demo...\n",rb_demo->name);
  if (rb_demo_init()<0) {
    return 1;
  }
  fprintf(stderr,
    "%s: Running. SIGINT%s to quit...\n",
    rb_demo->name,rb_demo->use_video?" or close window":""
  );
  if (rb_demo_main()<0) {
    rb_demo_quit(1);
    return 1;
  }
  rb_demo_quit(0);
  return 0;
}
