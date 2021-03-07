#include "rb_demo.h"
#include <unistd.h>

//TODO Generalize demos, make lots of them that we can call up by name.
// For now just doing one thing.

static int terminate=0;

static int _video_close(struct rb_video *video) {
  terminate=1;
  return 0;
}

int main(int argc,char **argv) {

  struct rb_video_delegate video_delegate={
    .winw=512,
    .winh=288,
    .fullscreen=0,
    .title="Rabbit Demo",
    .cb_close=_video_close,
  };
  struct rb_video *video=rb_video_new(0,&video_delegate);
  if (!video) {
    fprintf(stderr,"ERROR: Failed to instantiate default video driver.\n");
    return 1;
  }
  
  struct rb_framebuffer fb={0};
  
  while (!terminate) {
    if (rb_video_update(video)<0) return 1;
    if (rb_video_swap(video,&fb)<0) return 1;
    usleep(10000);
  }
  
  rb_video_del(video);
  return 0;
}
