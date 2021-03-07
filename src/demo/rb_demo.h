#ifndef RB_DEMO_H
#define RB_DEMO_H

#include "rabbit/rabbit.h"
#include "rabbit/rb_video.h"
#include "rabbit/rb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RB_DEFAULT_DEMO_NAME "xform"

#define RB_FOR_EACH_DEMO \
  _(manysprites) \
  _(xform)
  
/* Define these things before including this header, if you don't want the defaults.
 */
#ifndef RB_DEMO_USE_VIDEO
  #define RB_DEMO_USE_VIDEO     1
#endif
#if RB_DEMO_USE_CB_CLICK
  #define RB_DEMO_CBCLICK(_) _
#else
  #define RB_DEMO_CBCLICK(_) 0
#endif

extern struct rb_video *rb_demo_video;
extern struct rb_framebuffer rb_demo_fb;
extern int rb_demo_mousex;
extern int rb_demo_mousey;

struct rb_demo {
  const char *name;
  void (*quit)();
  int (*init)();
  int (*update)();
  int (*cb_click)(int x,int y,int value);
  int use_video;
};

#define RB_DEMO(_name) const struct rb_demo rb_demo_##_name={ \
  .name=#_name, \
  .quit=demo_##_name##_quit, \
  .init=demo_##_name##_init, \
  .update=demo_##_name##_update, \
  .cb_click=RB_DEMO_CBCLICK(demo_##_name##_cb_click), \
  .use_video=RB_DEMO_USE_VIDEO, \
};

#endif
