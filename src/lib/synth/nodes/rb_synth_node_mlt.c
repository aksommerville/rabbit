#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"

#define RB_GAIN_FLDID_main    0x01
#define RB_GAIN_FLDID_arg     0x02

/* Instance definition.
 */
 
struct rb_synth_node_config_mlt {
  struct rb_synth_node_config hdr;
  rb_sample_t arg;
};

struct rb_synth_node_runner_mlt {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  rb_sample_t *argv;
};

#define CONFIG ((struct rb_synth_node_config_mlt*)config)
#define RUNNER ((struct rb_synth_node_runner_mlt*)runner)
#define RCONFIG ((struct rb_synth_node_config_mlt*)(runner->config))

/* Init config.
 */
 
static int _rb_mlt_config_init(struct rb_synth_node_config *config) {
  CONFIG->arg=0.0f;
  return 0;
}

/* Ready config.
 */
 
static int _rb_mlt_config_ready(struct rb_synth_node_config *config) {
  if (!rb_synth_node_config_field_is_buffer(config,RB_GAIN_FLDID_main)) return -1;
  return 0;
}

/* Update runner.
 */
 
static void _rb_mlt_runner_update_v(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *main=RUNNER->mainv;
  rb_sample_t *arg=RUNNER->argv;
  for (;c-->0;main++,arg++) {
    (*main)*=(*arg);
  }
}
 
static void _rb_mlt_runner_update_s(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *main=RUNNER->mainv;
  for (;c-->0;main++) {
    (*main)*=RCONFIG->arg;
  }
}

/* Init runner.
 */
 
static int _rb_mlt_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  if (RUNNER->argv) runner->update=_rb_mlt_runner_update_v;
  else runner->update=_rb_mlt_runner_update_s;
  return 0;
}

/* Type definition.
 */
 
static const struct rb_synth_node_field _rb_mlt_fieldv[]={
  {
    .fldid=RB_GAIN_FLDID_main,
    .name="main",
    .desc="Input and output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_mlt*)0)->mainv,
  },
  {
    .fldid=RB_GAIN_FLDID_arg,
    .name="arg",
    .desc="Operand, scalar or vector.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_mlt*)0)->arg,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_mlt*)0)->argv,
  },
};

const struct rb_synth_node_type rb_synth_node_type_mlt={
  .ntid=RB_SYNTH_NTID_mlt,
  .name="mlt",
  .desc="Multiply a scalar or vector to a vector.",
  .config_objlen=sizeof(struct rb_synth_node_config_mlt),
  .runner_objlen=sizeof(struct rb_synth_node_runner_mlt),
  .fieldv=_rb_mlt_fieldv,
  .fieldc=sizeof(_rb_mlt_fieldv)/sizeof(struct rb_synth_node_field),
  .config_init=_rb_mlt_config_init,
  .config_ready=_rb_mlt_config_ready,
  .runner_init=_rb_mlt_runner_init,
};
