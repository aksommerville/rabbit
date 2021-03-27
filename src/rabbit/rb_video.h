/* rb_video.h
 * Interface with various platform-specific implementations.
 * Manages the window manager (if present) and video output.
 */
 
#ifndef RB_VIDEO_H
#define RB_VIDEO_H

struct rb_video;
struct rb_video_type;
struct rb_video_delegate;

/* Video driver instance.
 ************************************************************/
 
struct rb_video_delegate {
  void *userdata;
  
// Init parameters:
  int winw,winh;
  int fullscreen;
  const char *title;
  
// Callbacks. Implementation might not use them at all:
  int (*cb_close)(struct rb_video *video);
  int (*cb_resize)(struct rb_video *video);
  int (*cb_focus)(struct rb_video *video,int focus);
  int (*cb_key)(struct rb_video *video,int keycode,int value);
  int (*cb_text)(struct rb_video *video,int codepoint);
  int (*cb_mmotion)(struct rb_video *video,int x,int y);
  int (*cb_mbutton)(struct rb_video *video,int btnid,int value);
  int (*cb_mwheel)(struct rb_video *video,int dx,int dy);
};

struct rb_video {
  const struct rb_video_type *type;
  int refc;
  struct rb_video_delegate delegate;
  int winw,winh;
  int fullscreen;
};

/* Create a video driver.
 * On success, you have a window or have control of the monitor.
 * Null for (type) to select the default.
 */
struct rb_video *rb_video_new(
  const struct rb_video_type *type,
  const struct rb_video_delegate *delegate
);

void rb_video_del(struct rb_video *video);
int rb_video_ref(struct rb_video *video);

/* Poll for events and trigger the delegate's callbacks.
 * On non-window-manager systems, this should do nothing.
 */
int rb_video_update(struct rb_video *video);

/* Send (fb) to the screen and block until it gets there (ie vblank).
 * (fb) must have alphamode OPAQUE and dimensions (RB_FB_W,RB_FB_H).
 */
int rb_video_swap(struct rb_video *video,struct rb_image *fb);

/* 0=window, 1=fullscreen, -1=query
 * Returns actual state after change.
 */
int rb_video_set_fullscreen(struct rb_video *video,int fullscreen);

/* For X11 in particular, if you're taking input from joysticks,
 * call this frequently to tell the window manager we are still active.
 */
void rb_video_suppress_screensaver(struct rb_video *video);

/* Video driver types.
 ***********************************************************/
 
struct rb_video_type {
  const char *name;
  const char *desc;
  int objlen;
  void *singleton;
  void (*del)(struct rb_video *video);
  int (*init)(struct rb_video *video);
  int (*update)(struct rb_video *video);
  int (*swap)(struct rb_video *video,struct rb_image *fb); // REQUIRED
  int (*set_fullscreen)(struct rb_video *video,int fullscreen);
  void (*suppress_screensaver)(struct rb_video *video);
};

const struct rb_video_type *rb_video_type_by_name(const char *name,int namec);
const struct rb_video_type *rb_video_type_by_index(int p);

int rb_video_type_validate(const struct rb_video_type *type);

#endif
