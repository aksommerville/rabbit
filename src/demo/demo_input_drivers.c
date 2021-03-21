#include "rb_demo.h"
#include "rabbit/rb_input.h"

static struct rb_input **inputv=0;
static int inputc=0,inputa=0;

static int cb_enumerate(
  int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata
) {
  fprintf(stderr,"  %08x 0x%08x =%d (%d..%d)\n",btnid,hidusage,value,lo,hi);
  return 0;
}

static int cb_connect(struct rb_input *input,int devid) {
  fprintf(stderr,"%s %s %d\n",__func__,input->type->name,devid);
  int devix=rb_input_devix_from_devid(input,devid);
  
  struct rb_input_device_description desc={0};
  if (rb_input_device_get_description(&desc,input,devix)<0) {
    fprintf(stderr,"...failed to acquire device description\n");
  } else {
    fprintf(stderr,"  Vendor:  0x%04x\n",desc.vid);
    fprintf(stderr,"  Product: 0x%04x\n",desc.pid);
    fprintf(stderr,"  Version: 0x%04x\n",desc.version);
    fprintf(stderr,"  Name: '%.*s'\n",desc.namec,desc.name);
  }
  
  if (rb_input_device_enumerate(input,devix,cb_enumerate,0)<0) {
    fprintf(stderr,"...failed to enumerate buttons\n");
  }
  
  return 0;
}

static int cb_disconnect(struct rb_input *input,int devid) {
  fprintf(stderr,"%s %s %d\n",__func__,input->type->name,devid);
  return 0;
}

static int cb_event(struct rb_input *input,int devid,int btnid,int value) {
  fprintf(stderr,"%s %s %d.%d=%d\n",__func__,input->type->name,devid,btnid,value);
  return 0;
}

static int demo_input_drivers_init() {

  struct rb_input_delegate delegate={
    .cb_connect=cb_connect,
    .cb_disconnect=cb_disconnect,
    .cb_event=cb_event,
  };

  int p=0;
  const struct rb_input_type *type;
  for (;type=rb_input_type_by_index(p);p++) {
    fprintf(stderr,"Initialize driver '%s'...\n",type->name);
    
    if (inputc>=inputa) {
      int na=inputa+8;
      if (na>INT_MAX/sizeof(void*)) return -1;
      void *nv=realloc(inputv,sizeof(void*)*na);
      if (!nv) return -1;
      inputv=nv;
      inputa=na;
    }
    
    struct rb_input *input=rb_input_new(type,&delegate);
    if (!input) {
      fprintf(stderr,"...FAILED!\n");
      continue;
    }
    fprintf(stderr,"...connected\n");
    inputv[inputc++]=input;
  }

  return 0;
}

static void demo_input_drivers_quit() {
  if (inputv) {
    while (inputc-->0) rb_input_del(inputv[inputc]);
    free(inputv);
  }
  inputv=0;
  inputc=0;
  inputa=0;
}

static int demo_input_drivers_update() {
  int i=inputc;
  while (i-->0) {
    if (rb_input_update(inputv[i])<0) {
      fprintf(stderr,"Error updating input driver '%s'! Dropping.\n",inputv[i]->type->name);
      rb_input_del(inputv[i]);
      inputc--;
      memmove(inputv+i,inputv+i+1,sizeof(void*)*(inputc-i));
    }
  }
  return 1;
}

RB_DEMO(input_drivers)
