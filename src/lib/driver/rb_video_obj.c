#include "rabbit/rb_internal.h"
#include "rabbit/rb_video.h"
#include "rabbit/rb_image.h"

/* New.
 */
 
struct rb_video *rb_video_new(
  const struct rb_video_type *type,
  const struct rb_video_delegate *delegate
) {
  if (!type) {
    if (!(type=rb_video_type_by_index(0))) return 0;
  }
  
  struct rb_video *video=0;
  if (type->singleton) {
    video=type->singleton;
    if (video->refc) return 0;
  } else {
    if (!(video=calloc(1,type->objlen))) return 0;
  }
  video->type=type;
  video->refc=1;
  
  if (delegate) {
    video->delegate=*delegate;
    video->winw=delegate->winw;
    video->winh=delegate->winh;
    video->fullscreen=delegate->fullscreen;
  } else {
    video->winw=RB_FB_W*2;
    video->winh=RB_FB_H*2;
  }
  
  if (type->init) {
    if (type->init(video)<0) {
      rb_video_del(video);
      return 0;
    }
  }
  
  return video;
}

/* Delete.
 */

void rb_video_del(struct rb_video *video) {
  if (!video) return;
  if (video->refc-->1) return;
  if (video->type->del) video->type->del(video);
  if (video==video->type->singleton) memset(video,0,video->type->objlen);
  else free(video);
}

/* Retain.
 */
 
int rb_video_ref(struct rb_video *video) {
  if (!video) return -1;
  if (video->refc<1) return -1;
  if (video->refc==INT_MAX) return -1;
  video->refc++;
  return 0;
}

/* Hook wrappers.
 */
 
int rb_video_update(struct rb_video *video) {
  if (!video->type->update) return 0;
  return video->type->update(video);
}

int rb_video_swap(struct rb_video *video,struct rb_framebuffer *fb) {
  return video->type->swap(video,fb);
}

int rb_video_set_fullscreen(struct rb_video *video,int fullscreen) {
  if (fullscreen<0) return video->fullscreen;
  if (video->type->set_fullscreen) {
    if (video->type->set_fullscreen(video,fullscreen)<0) return -1;
  }
  return video->fullscreen;
}

void rb_video_suppress_screensaver(struct rb_video *video) {
  if (video->type->suppress_screensaver) {
    video->type->suppress_screensaver(video);
  }
}
