#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"
#include <math.h>

#define RB_ENV_FLDID_main 0x01
#define RB_ENV_FLDID_mode 0x02

/* Instance definition.
 */
 
struct rb_synth_node_config_env {
  struct rb_synth_node_config hdr;
  int mode;
};

struct rb_synth_node_runner_env {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
};

#define CONFIG ((struct rb_synth_node_config_env*)config)
#define RUNNER ((struct rb_synth_node_runner_env*)runner)
#define RCONFIG ((struct rb_synth_node_config_env*)(runner->config))

/* Cleanup.
 */
 
static void _rb_env_config_del(struct rb_synth_node_config *config) {
}

static void _rb_env_runner_del(struct rb_synth_node_runner *runner) {
}

/* Init config.
 */
 
static int _rb_env_config_init(struct rb_synth_node_config *config) {
  CONFIG->mode=0;
  return 0;
}

/* Ready config.
 */
 
static int _rb_env_config_ready(struct rb_synth_node_config *config) {
  return 0;
}

/* Update.
 */
 
static void _rb_env_update(struct rb_synth_node_runner *runner,int c) {
  //TODO
}

/* Init runner.
 */
 
static int _rb_env_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  runner->update=_rb_env_update;
  return 0;
}

/* Get duration.
 */
 
static int _rb_env_runner_get_duration(struct rb_synth_node_runner *runner) {
  return 10000;//TODO
}

/* Fields.
 */
 
static const struct rb_synth_node_field _rb_env_fieldv[]={
  {
    .fldid=RB_ENV_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_env*)0)->mainv,
  },
  {
    .fldid=RB_ENV_FLDID_mode,
    .name="mode",
    .desc="How do we deal with prior content in main? Default multiply.",
    .enumlabels="mlt,set,add",
    .config_offseti=(uintptr_t)&((struct rb_synth_node_config_env*)0)->mode,
  },
};

/* Type definition.
 */
 
const struct rb_synth_node_type rb_synth_node_type_env={
  .ntid=RB_SYNTH_NTID_env,
  .name="env",
  .desc="Envelope",
  .flags=0,
  .config_objlen=sizeof(struct rb_synth_node_config_env),
  .runner_objlen=sizeof(struct rb_synth_node_runner_env),
  .fieldv=_rb_env_fieldv,
  .fieldc=sizeof(_rb_env_fieldv)/sizeof(struct rb_synth_node_field),
  .config_del=_rb_env_config_del,
  .runner_del=_rb_env_runner_del,
  .config_init=_rb_env_config_init,
  .config_ready=_rb_env_config_ready,
  .runner_init=_rb_env_runner_init,
  .runner_get_duration=_rb_env_runner_get_duration,
};
