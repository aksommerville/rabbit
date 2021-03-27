#ifndef RB_DEMO_H
#define RB_DEMO_H

#include "rabbit/rb_internal.h"
#include "rabbit/rb_video.h"
#include "rabbit/rb_audio.h"
#include "rabbit/rb_image.h"
#include "rabbit/rb_synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RB_DEFAULT_DEMO_NAME "halfscroll"

#define RB_FOR_EACH_DEMO \
  _(manysprites) \
  _(xform) \
  _(midiin) \
  _(midiredline) \
  _(vmgr) \
  _(input_drivers) \
  _(inmgr) \
  _(halfscroll)
  
/* Define these things before including this header, if you don't want the defaults.
 */
#ifndef RB_DEMO_USE_VIDEO
  #define RB_DEMO_USE_VIDEO 1
#endif
#ifndef RB_DEMO_USE_AUDIO
  // Ask for audio, we also make a synth and connect them.
  // Wrapper does not configure the synth at all.
  #define RB_DEMO_USE_AUDIO 1
#endif
#if RB_DEMO_USE_CB_CLICK
  #define RB_DEMO_CBCLICK(_) _
#else
  #define RB_DEMO_CBCLICK(_) 0
#endif
#if RB_DEMO_USE_CB_KEY
  #define RB_DEMO_CBKEY(_) _
#else
  #define RB_DEMO_CBKEY(_) 0
#endif

extern struct rb_video *rb_demo_video;
extern struct rb_audio *rb_demo_audio;
extern struct rb_image *rb_demo_fb;
extern struct rb_image *rb_demo_override_fb; // demo may set this to provide its own fb
extern struct rb_synth *rb_demo_synth;
extern int rb_demo_mousex;
extern int rb_demo_mousey;

struct rb_demo {
  const char *name;
  void (*quit)();
  int (*init)();
  int (*update)();
  int (*cb_click)(int x,int y,int value);
  int (*cb_key)(int keycode,int value);
  int use_video;
  int use_audio;
};

#define RB_DEMO(_name) const struct rb_demo rb_demo_##_name={ \
  .name=#_name, \
  .quit=demo_##_name##_quit, \
  .init=demo_##_name##_init, \
  .update=demo_##_name##_update, \
  .cb_click=RB_DEMO_CBCLICK(demo_##_name##_cb_click), \
  .cb_key=RB_DEMO_CBKEY(demo_##_name##_cb_key), \
  .use_video=RB_DEMO_USE_VIDEO, \
  .use_audio=RB_DEMO_USE_AUDIO, \
};

#endif
