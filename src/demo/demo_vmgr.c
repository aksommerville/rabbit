#include "rb_demo.h"
#include "rabbit/rb_vmgr.h"

static struct rb_vmgr *vmgr=0;

static int demo_vmgr_init() {
  if (!(vmgr=rb_vmgr_new())) return -1;
  rb_demo_override_fb=&vmgr->fb;
  return 0;
}

static void demo_vmgr_quit() {
  rb_vmgr_del(vmgr);
  vmgr=0;
}

static int demo_vmgr_update() {
  if (rb_vmgr_render(vmgr)!=rb_demo_override_fb) {
    fprintf(stderr,"rb_vmgr_render() failed\n");
    return -1;
  }
  return 1;
}

RB_DEMO(vmgr)
