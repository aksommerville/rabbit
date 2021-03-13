#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"

#define RB_GAIN_FLDID_main    0x01
#define RB_GAIN_FLDID_gain    0x02
#define RB_GAIN_FLDID_clip    0x03
#define RB_GAIN_FLDID_gate    0x04

/* Instance definition.
 */
 
struct rb_synth_node_config_gain {
  struct rb_synth_node_config hdr;
  rb_sample_t gain,clip,gate;
};

struct rb_synth_node_runner_gain {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  rb_sample_t *gainv;
};

#define CONFIG ((struct rb_synth_node_config_gain*)config)
#define RUNNER ((struct rb_synth_node_runner_gain*)runner)
#define RCONFIG ((struct rb_synth_node_config_gain*)(runner->config))

/* Init config.
 */
 
static int _rb_gain_config_init(struct rb_synth_node_config *config) {
  CONFIG->gain=1.0f;
  CONFIG->clip=1.0f;
  CONFIG->gate=0.0f;
  return 0;
}

/* Ready config.
 */
 
static int _rb_gain_config_ready(struct rb_synth_node_config *config) {
  if (!rb_synth_node_config_field_is_buffer(config,RB_GAIN_FLDID_main)) return -1;
  return 0;
}

/* Update runner.
 */
 
static void _rb_gain_runner_update_v(struct rb_synth_node_runner *runner,int c) {
  const rb_sample_t clipp=RCONFIG->clip;
  const rb_sample_t clipn=-clipp;
  const rb_sample_t gatep=RCONFIG->gate;
  const rb_sample_t gaten=-gatep;
  rb_sample_t *main=RUNNER->mainv;
  rb_sample_t *gain=RUNNER->gainv;
  for (;c-->0;main++,gain++) {
    (*main)*=(*gain);
         if ((*main)>=clipp) (*main)=clipp;
    else if ((*main)>=gatep) ;
    else if ((*main)> gaten) (*main)=0.0f;
    else if ((*main)> clipn) ;
    else                     (*main)=clipn;
  }
}
 
static void _rb_gain_runner_update_s(struct rb_synth_node_runner *runner,int c) {
  const rb_sample_t clipp=RCONFIG->clip;
  const rb_sample_t clipn=-clipp;
  const rb_sample_t gatep=RCONFIG->gate;
  const rb_sample_t gaten=-gatep;
  const rb_sample_t gain=RCONFIG->gain;
  rb_sample_t *main=RUNNER->mainv;
  for (;c-->0;main++) {
    (*main)*=gain;
         if ((*main)>=clipp) (*main)=clipp;
    else if ((*main)>=gatep) ;
    else if ((*main)> gaten) (*main)=0.0f;
    else if ((*main)> clipn) ;
    else                     (*main)=clipn;
  }
}

/* Init runner.
 */
 
static int _rb_gain_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {

  if (!RUNNER->mainv) return -1;
  
  if (RUNNER->gainv) runner->update=_rb_gain_runner_update_v;
  else runner->update=_rb_gain_runner_update_s;
  return 0;
}

/* Type definition.
 */
 
static const struct rb_synth_node_field _rb_gain_fieldv[]={
  {
    .fldid=RB_GAIN_FLDID_main,
    .name="main",
    .desc="Input and output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_gain*)0)->mainv,
  },
  {
    .fldid=RB_GAIN_FLDID_gain,
    .name="gain",
    .desc="Multiplier.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_gain*)0)->gain,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_gain*)0)->gainv,
  },
  {
    .fldid=RB_GAIN_FLDID_clip,
    .name="clip",
    .desc="Maximum output level, anything beyond this clamps.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_gain*)0)->clip,
  },
  {
    .fldid=RB_GAIN_FLDID_gate,
    .name="gate",
    .desc="Minimum output level, anything below this clamps to zero.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_gain*)0)->gate,
  },
};

const struct rb_synth_node_type rb_synth_node_type_gain={
  .ntid=RB_SYNTH_NTID_gain,
  .name="gain",
  .desc="Gain, clip, and gate.",
  .config_objlen=sizeof(struct rb_synth_node_config_gain),
  .runner_objlen=sizeof(struct rb_synth_node_runner_gain),
  .fieldv=_rb_gain_fieldv,
  .fieldc=sizeof(_rb_gain_fieldv)/sizeof(struct rb_synth_node_field),
  .config_init=_rb_gain_config_init,
  .config_ready=_rb_gain_config_ready,
  .runner_init=_rb_gain_runner_init,
};
