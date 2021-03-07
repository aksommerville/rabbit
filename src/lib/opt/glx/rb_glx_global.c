#include "rb_glx_internal.h"
#include <stdlib.h>
#include <string.h>
 
/* Initialize X11 (display is already open).
 */
 
static int rb_glx_startup(struct rb_video *video) {
  
  #define GETATOM(tag) VIDEO->atom_##tag=XInternAtom(VIDEO->dpy,#tag,0);
  GETATOM(WM_PROTOCOLS)
  GETATOM(WM_DELETE_WINDOW)
  GETATOM(_NET_WM_STATE)
  GETATOM(_NET_WM_STATE_FULLSCREEN)
  GETATOM(_NET_WM_STATE_ADD)
  GETATOM(_NET_WM_STATE_REMOVE)
  GETATOM(_NET_WM_ICON)
  #undef GETATOM

  int attrv[]={
    GLX_X_RENDERABLE,1,
    GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE,GLX_TRUE_COLOR,
    GLX_RED_SIZE,8,
    GLX_GREEN_SIZE,8,
    GLX_BLUE_SIZE,8,
    GLX_ALPHA_SIZE,0,
    GLX_DEPTH_SIZE,0,
    GLX_STENCIL_SIZE,0,
    GLX_DOUBLEBUFFER,1,
  0};
  
  if (!glXQueryVersion(VIDEO->dpy,&VIDEO->glx_version_major,&VIDEO->glx_version_minor)) {
    return -1;
  }
  
  int fbc=0;
  GLXFBConfig *configv=glXChooseFBConfig(VIDEO->dpy,VIDEO->screen,attrv,&fbc);
  if (!configv||(fbc<1)) {
    return -1;
  }
  GLXFBConfig config=configv[0];
  XFree(configv);
  
  XVisualInfo *vi=glXGetVisualFromFBConfig(VIDEO->dpy,config);
  if (!vi) {
    return -1;
  }
  
  XSetWindowAttributes wattr={
    .event_mask=
      StructureNotifyMask|
      KeyPressMask|KeyReleaseMask|
      ButtonPressMask|ButtonReleaseMask|
      PointerMotionMask|
      FocusChangeMask|
    0,
  };
  wattr.colormap=XCreateColormap(VIDEO->dpy,RootWindow(VIDEO->dpy,vi->screen),vi->visual,AllocNone);
  
  if (!(VIDEO->win=XCreateWindow(
    VIDEO->dpy,RootWindow(VIDEO->dpy,vi->screen),
    0,0,video->winw,video->winh,0,
    vi->depth,InputOutput,vi->visual,
    CWBorderPixel|CWColormap|CWEventMask,&wattr
  ))) {
    XFree(vi);
    return -1;
  }
  
  XFree(vi);
  
  if (video->delegate.title) {
    XStoreName(VIDEO->dpy,VIDEO->win,video->delegate.title);
  }
  
  if (video->delegate.fullscreen) {
    XChangeProperty(
      VIDEO->dpy,VIDEO->win,
      VIDEO->atom__NET_WM_STATE,
      XA_ATOM,32,PropModeReplace,
      (unsigned char*)&VIDEO->atom__NET_WM_STATE_FULLSCREEN,1
    );
    video->fullscreen=1;
  }
  
  XMapWindow(VIDEO->dpy,VIDEO->win);
  
  if (!(VIDEO->ctx=glXCreateNewContext(VIDEO->dpy,config,GLX_RGBA_TYPE,0,1))) {
    return -1;
  }
  glXMakeCurrent(VIDEO->dpy,VIDEO->win,VIDEO->ctx);
  
  XSync(VIDEO->dpy,0);
  
  XSetWMProtocols(VIDEO->dpy,VIDEO->win,&VIDEO->atom_WM_DELETE_WINDOW,1);

  return 0;
}
 
/* Init.
 */
 
static int _rb_glx_init(struct rb_video *video) {

  VIDEO->cursor_visible=1;
  VIDEO->focus=1;
  
  if (!(VIDEO->dpy=XOpenDisplay(0))) {
    return -1;
  }
  VIDEO->screen=DefaultScreen(VIDEO->dpy);
  
  if (rb_glx_startup(video)<0) {
    return -1;
  }
  
  glEnable(GL_POINT_SPRITE);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE);

  return 0;
}

/* Quit.
 */

static void _rb_glx_del(struct rb_video *video) {
  if (VIDEO->ctx) {
    glXMakeCurrent(VIDEO->dpy,0,0);
    glXDestroyContext(VIDEO->dpy,VIDEO->ctx);
  }
  if (VIDEO->dpy) {
    XCloseDisplay(VIDEO->dpy);
  }
}

/* Frame control.
 */

static int _rb_glx_swap(struct rb_video *video,struct rb_framebuffer *fb) {

  //TODO copy fb to window

  glXSwapBuffers(VIDEO->dpy,VIDEO->win);
  VIDEO->screensaver_inhibited=0;
  return 0;
}

/* Toggle fullscreen.
 */

static int rb_glx_enter_fullscreen(struct rb_video *video) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=VIDEO->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=VIDEO->win,
      .data={.l={
        1,
        VIDEO->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(VIDEO->dpy,RootWindow(VIDEO->dpy,VIDEO->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(VIDEO->dpy);
  video->fullscreen=1;
  return 0;
}

static int rb_glx_exit_fullscreen(struct rb_video *video) {
  XEvent evt={
    .xclient={
      .type=ClientMessage,
      .message_type=VIDEO->atom__NET_WM_STATE,
      .send_event=1,
      .format=32,
      .window=VIDEO->win,
      .data={.l={
        0,
        VIDEO->atom__NET_WM_STATE_FULLSCREEN,
      }},
    }
  };
  XSendEvent(VIDEO->dpy,RootWindow(VIDEO->dpy,VIDEO->screen),0,SubstructureNotifyMask|SubstructureRedirectMask,&evt);
  XFlush(VIDEO->dpy);
  video->fullscreen=0;
  return 0;
}
 
static int _rb_glx_set_fullscreen(struct rb_video *video,int state) {
  if (state>0) {
    if (video->fullscreen) return 1;
    if (rb_glx_enter_fullscreen(video)<0) return -1;
  } else if (state==0) {
    if (!video->fullscreen) return 0;
    if (rb_glx_exit_fullscreen(video)<0) return -1;
  }
  return video->fullscreen;
}

/* Cursor visibility.
 */

static int rb_glx_set_cursor_invisible(struct rb_video *video) {
  XColor color;
  Pixmap pixmap=XCreateBitmapFromData(VIDEO->dpy,VIDEO->win,"\0\0\0\0\0\0\0\0",1,1);
  Cursor cursor=XCreatePixmapCursor(VIDEO->dpy,pixmap,pixmap,&color,&color,0,0);
  XDefineCursor(VIDEO->dpy,VIDEO->win,cursor);
  XFreeCursor(VIDEO->dpy,cursor);
  XFreePixmap(VIDEO->dpy,pixmap);
  return 0;
}

static int rb_glx_set_cursor_visible(struct rb_video *video) {
  XDefineCursor(VIDEO->dpy,VIDEO->win,None);
  return 0;
}

int rb_glx_show_cursor(struct rb_video *video,int show) {
  if (!VIDEO->dpy) return -1;
  if (show) {
    if (VIDEO->cursor_visible) return 0;
    if (rb_glx_set_cursor_visible(video)<0) return -1;
    VIDEO->cursor_visible=1;
  } else {
    if (!VIDEO->cursor_visible) return 0;
    if (rb_glx_set_cursor_invisible(video)<0) return -1;
    VIDEO->cursor_visible=0;
  }
  return 0;
}

int rb_glx_get_cursor(struct rb_video *video) {
  return VIDEO->cursor_visible;
}

/* Set window icon. TODO expose this via rb_video_delegate
 */
 
static void rb_glx_copy_pixels(long *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) {
    #if BYTE_ORDER==BIG_ENDIAN
      /* https://standards.freedesktop.org/wm-spec/wm-spec-1.3.html
       * """
       * This is an array of 32bit packed CARDINAL ARGB with high byte being A, low byte being B.
       * The first two cardinals are width, height. Data is in rows, left to right and top to bottom.
       * """
       * I take this to mean that big-endian should work the same as little-endian.
       * But I'm nervous about it because:
       *  - I don't have any big-endian machines handy for testing.
       *  - The data type is "long" which is not always "32bit" as they say. (eg it is 64 on my box)
       */
      *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
    #else
      *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
    #endif
  }
}
 
int rb_glx_set_icon(struct rb_video *video,const void *rgba,int w,int h) {
  if (!rgba||(w<1)||(h<1)) return -1;
  int length=2+w*h;
  long *pixels=malloc(sizeof(long)*length);
  if (!pixels) return -1;
  pixels[0]=w;
  pixels[1]=h;
  rb_glx_copy_pixels(pixels+2,rgba,w*h);
  XChangeProperty(
    VIDEO->dpy,VIDEO->win,
    VIDEO->atom__NET_WM_ICON,
    XA_CARDINAL,32,PropModeReplace,
    (unsigned char*)pixels,length
  );
  free(pixels);
  return 0;
}

/* Inhibit screensaver.
 */
 
void _rb_glx_suppress_screensaver(struct rb_video *video) {
  if (VIDEO->screensaver_inhibited) return;
  XForceScreenSaver(VIDEO->dpy,ScreenSaverReset);
  VIDEO->screensaver_inhibited=1;
}

/* Type definition.
 */
 
struct rb_video_glx _rb_glx_singleton={0};

const struct rb_video_type rb_video_type_glx={
  .name="glx",
  .desc="X11 with Xlib and GLX, for Linux.",
  .objlen=sizeof(struct rb_video_glx),
  .singleton=&_rb_glx_singleton,
  .del=_rb_glx_del,
  .init=_rb_glx_init,
  .update=_rb_glx_update,
  .swap=_rb_glx_swap,
  .set_fullscreen=_rb_glx_set_fullscreen,
  .suppress_screensaver=_rb_glx_suppress_screensaver,
};
