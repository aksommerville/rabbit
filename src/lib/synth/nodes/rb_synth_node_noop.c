#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"

struct rb_synth_node_runner_noop {
  struct rb_synth_node_runner hdr;
  rb_sample_t *main;
};

static void _rb_noop_update(struct rb_synth_node_runner *runner,int c) {
}

static int _rb_noop_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  runner->update=_rb_noop_update;
  return 0;
}

static int _rb_noop_runner_get_duration(struct rb_synth_node_runner *runner) {
  return 1;
}

static const struct rb_synth_node_field _rb_noop_fieldv[]={
  {
    .fldid=0x01,
    .name="main",
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_noop*)0)->main,
  },
};

const struct rb_synth_node_type rb_synth_node_type_noop={
  .ntid=RB_SYNTH_NTID_noop,
  .name="noop",
  .desc="Placeholder do-nothing node.",
  .flags=RB_SYNTH_NODE_TYPE_PROGRAM,
  .config_objlen=sizeof(struct rb_synth_node_config),
  .runner_objlen=sizeof(struct rb_synth_node_runner_noop),
  .fieldv=_rb_noop_fieldv,
  .fieldc=sizeof(_rb_noop_fieldv)/sizeof(struct rb_synth_node_field),
  .runner_init=_rb_noop_runner_init,
  .runner_get_duration=_rb_noop_runner_get_duration,
};
