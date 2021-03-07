#include "rb_demo.h"
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <math.h>

/* Globals.
 */
 
static const struct rb_demo *rb_demo=0;
static volatile int rb_sigc=0;
static volatile int rb_terminate=0;
struct rb_video *rb_demo_video=0;
struct rb_framebuffer rb_demo_fb={0};
struct rb_audio *rb_demo_audio=0;
int rb_demo_mousex=0;
int rb_demo_mousey=0;
static double rb_demo_starttime=0.0;
static int rb_demo_framec=0;

/* Current real time.
 */
 
static double rb_demo_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+tv.tv_usec/1000000.0f;
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
  
  if (rb_demo_audio) {
    rb_audio_del(rb_demo_audio);
    rb_demo_audio=0;
  }

  rb_demo->quit();
  
  if (rb_demo_video) {
    rb_video_del(rb_demo_video);
    rb_demo_video=0;
  }
  
  if (status) {
    fprintf(stderr,"%s: Terminate due to error.\n",rb_demo->name);
  } else if (rb_demo_framec>0) {
    double elapsed=endtime-rb_demo_starttime;
    double rate=rb_demo_framec/elapsed;
    fprintf(stderr,
      "%s: Normal exit. %d frames in %.03fs, average %.03f Hz.\n",
      rb_demo->name,rb_demo_framec,elapsed,rate
    );
  } else {
    fprintf(stderr,"%s: Normal exit.\n",rb_demo->name);
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
  memset(v,0,c<<1);
  return 0;
}

/* Initialize.
 */
 
static int rb_demo_init() {

  if (rb_demo->use_video) {
    struct rb_video_delegate delegate={
      .title=rb_demo->name,
      .cb_close=rb_demo_cb_close,
      .cb_mmotion=rb_demo_cb_mmotion,
      .cb_mbutton=rb_demo_cb_mbutton,
    };
    if (!(rb_demo_video=rb_video_new(0,&delegate))) {
      fprintf(stderr,"Failed to initialize default video driver.\n");
      return -1;
    }
  }
  
  if (rb_demo->use_audio) {
    struct rb_audio_delegate delegate={
      .rate=44100,
      .chanc=2,
      .cb_pcm_out=rb_demo_cb_pcm_out,
    };
    if (!(rb_demo_audio=rb_audio_new(0,&delegate))) {
      fprintf(stderr,"Failed to initialize default audio driver.\n");
      return -1;
    }
  }

  if (rb_demo->init()<0) {
    fprintf(stderr,"%s: init failed\n",rb_demo->name);
    return -1;
  }
  
  rb_demo_starttime=rb_demo_now();
  
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
      if (rb_video_swap(rb_demo_video,&rb_demo_fb)<0) {
        fprintf(stderr,"Video '%s': swap failed\n",rb_demo_video->type->name);
        return -1;
      }
    }
    //TODO Need a global timing regulator.
    usleep(10000);

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
