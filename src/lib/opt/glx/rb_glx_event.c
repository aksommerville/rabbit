#include "rb_glx_internal.h"

/* Key press, release, or repeat.
 */
 
static int rb_glx_evt_key(struct rb_video *video,XKeyEvent *evt,int value) {

  /* Pass the raw keystroke. */
  if (video->delegate.cb_key) {
    KeySym keysym=XkbKeycodeToKeysym(VIDEO->dpy,evt->keycode,0,0);
    if (keysym) {
      int keycode=rb_glx_usb_usage_from_keysym((int)keysym);
      if (keycode) {
        if (video->delegate.cb_key(video,keycode,value)<0) return -1;
      }
    }
  }
  
  /* Pass text if press or repeat, and text can be acquired. */
  if (value&&video->delegate.cb_text) {
    int shift=(evt->state&ShiftMask)?1:0;
    KeySym tkeysym=XkbKeycodeToKeysym(VIDEO->dpy,evt->keycode,0,shift);
    if (shift&&!tkeysym) { // If pressing shift makes this key "not a key anymore", fuck that and pretend shift is off
      tkeysym=XkbKeycodeToKeysym(VIDEO->dpy,evt->keycode,0,0);
    }
    if (tkeysym) {
      int codepoint=rb_glx_codepoint_from_keysym(tkeysym);
      if (codepoint) {
        if (video->delegate.cb_text(video,codepoint)<0) return -1;
      }
    }
  }
  
  return 0;
}

/* Mouse events.
 */
 
static int rb_glx_evt_mbtn(struct rb_video *video,XButtonEvent *evt,int value) {
  switch (evt->button) {
    case 1: if (video->delegate.cb_mbutton) return video->delegate.cb_mbutton(video,1,value); break;
    case 2: if (video->delegate.cb_mbutton) return video->delegate.cb_mbutton(video,3,value); break;
    case 3: if (video->delegate.cb_mbutton) return video->delegate.cb_mbutton(video,2,value); break;
    case 4: if (value&&video->delegate.cb_mwheel) return video->delegate.cb_mwheel(video,0,-1); break;
    case 5: if (value&&video->delegate.cb_mwheel) return video->delegate.cb_mwheel(video,0,1); break;
    case 6: if (value&&video->delegate.cb_mwheel) return video->delegate.cb_mwheel(video,-1,0); break;
    case 7: if (value&&video->delegate.cb_mwheel) return video->delegate.cb_mwheel(video,1,0); break;
  }
  return 0;
}

static int rb_glx_evt_mmotion(struct rb_video *video,XMotionEvent *evt) {
  if (video->delegate.cb_mmotion) {
    if (video->delegate.cb_mmotion(video,evt->x,evt->y)<0) return -1;
  }
  return 0;
}

/* Client message.
 */
 
static int rb_glx_evt_client(struct rb_video *video,XClientMessageEvent *evt) {
  if (evt->message_type==VIDEO->atom_WM_PROTOCOLS) {
    if (evt->format==32) {
      if (evt->data.l[0]==VIDEO->atom_WM_DELETE_WINDOW) {
        if (video->delegate.cb_close) {
          return video->delegate.cb_close(video);
        }
      }
    }
  }
  return 0;
}

/* Configuration event (eg resize).
 */
 
static int rb_glx_evt_configure(struct rb_video *video,XConfigureEvent *evt) {
  int nw=evt->width,nh=evt->height;
  if ((nw!=video->winw)||(nh!=video->winh)) {
    video->winw=nw;
    video->winh=nh;
    if (video->delegate.cb_resize) {
      if (video->delegate.cb_resize(video)<0) {
        return -1;
      }
    }
  }
  return 0;
}

/* Focus.
 */
 
static int rb_glx_evt_focus(struct rb_video *video,XFocusInEvent *evt,int value) {
  if (value==VIDEO->focus) return 0;
  VIDEO->focus=value;
  if (video->delegate.cb_focus) {
    return video->delegate.cb_focus(video,value);
  }
  return 0;
}

/* Dispatch single event.
 */
 
static int rb_glx_receive_event(struct rb_video *video,XEvent *evt) {
  if (!evt) return -1;
  switch (evt->type) {
  
    case KeyPress: return rb_glx_evt_key(video,&evt->xkey,1);
    case KeyRelease: return rb_glx_evt_key(video,&evt->xkey,0);
    case KeyRepeat: return rb_glx_evt_key(video,&evt->xkey,2);
    
    case ButtonPress: return rb_glx_evt_mbtn(video,&evt->xbutton,1);
    case ButtonRelease: return rb_glx_evt_mbtn(video,&evt->xbutton,0);
    case MotionNotify: return rb_glx_evt_mmotion(video,&evt->xmotion);
    
    case ClientMessage: return rb_glx_evt_client(video,&evt->xclient);
    
    case ConfigureNotify: return rb_glx_evt_configure(video,&evt->xconfigure);
    
    case FocusIn: return rb_glx_evt_focus(video,&evt->xfocus,1);
    case FocusOut: return rb_glx_evt_focus(video,&evt->xfocus,0);
    
  }
  return 0;
}

/* Update.
 */
 
int _rb_glx_update(struct rb_video *video) {
  int evtc=XEventsQueued(VIDEO->dpy,QueuedAfterFlush);
  while (evtc-->0) {
    XEvent evt={0};
    XNextEvent(VIDEO->dpy,&evt);
    
    /* If we detect an auto-repeated key, drop one of the events, and turn the other into KeyRepeat.
     * This is a hack to force single events for key repeat.
     */
    if ((evtc>0)&&(evt.type==KeyRelease)) {
      XEvent next={0};
      XNextEvent(VIDEO->dpy,&next);
      evtc--;
      if ((next.type==KeyPress)&&(evt.xkey.keycode==next.xkey.keycode)&&(evt.xkey.time>=next.xkey.time-RB_GLX_KEY_REPEAT_INTERVAL)) {
        evt.type=KeyRepeat;
        if (rb_glx_receive_event(video,&evt)<0) return -1;
      } else {
        if (rb_glx_receive_event(video,&evt)<0) return -1;
        if (rb_glx_receive_event(video,&next)<0) return -1;
      }
    } else {
      if (rb_glx_receive_event(video,&evt)<0) return -1;
    }
  }
  return 0;
}
