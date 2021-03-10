#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"
#include <math.h>

#define RB_BEEP_FLDID_main    0x01
#define RB_BEEP_FLDID_level   0x02

#define RB_BEEP_DURATION_MS 200
#define RB_BEEP_PEAK_TIME_MS 20

/* Instance definition.
 */
 
struct rb_synth_node_config_beep {
  struct rb_synth_node_config hdr;
  rb_sample_t level;
};

struct rb_synth_node_runner_beep {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  rb_sample_t p;
  rb_sample_t dp;
  rb_sample_t v; // 1,-1
  rb_sample_t level;
  rb_sample_t dlevel;
  int dc;
  int duration;
};

#define CONFIG ((struct rb_synth_node_config_beep*)config)
#define RUNNER ((struct rb_synth_node_runner_beep*)runner)
#define RCONFIG ((struct rb_synth_node_config_beep*)(runner->config))

/* Init config.
 */
 
static int _rb_beep_config_init(struct rb_synth_node_config *config) {
  CONFIG->level=0.15f;
  return 0;
}

/* Ready config.
 */
 
static int _rb_beep_config_ready(struct rb_synth_node_config *config) {
  if (rb_synth_node_config_field_is_buffer(config,RB_BEEP_FLDID_main)<0) return -1;
  return 0;
}

/* Update runner.
 */
 
static void _rb_beep_runner_update(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *main=RUNNER->mainv;
  for (;c-->0;main++) {
  
    (*main)=RUNNER->level*RUNNER->v;
    
    if (RUNNER->dc-->0) {
      RUNNER->level+=RUNNER->dlevel;
    } else {
      RUNNER->dc=RUNNER->duration-(runner->config->synth->rate*RB_BEEP_PEAK_TIME_MS)/1000;
      if (RUNNER->dc<1) RUNNER->dc=1;
      RUNNER->dlevel=-RUNNER->level/RUNNER->dc;
    }
    
    RUNNER->p+=RUNNER->dp;
    if (RUNNER->p>=1.0f) {
      RUNNER->p-=1.0f;
      RUNNER->v*=-1.0f;
    }
  }
}

/* Init runner.
 */
 
static int _rb_beep_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {

  if (!RUNNER->mainv) return -1;
  
  RUNNER->level=0.0f;
  RUNNER->v=1.0f;
  
  RUNNER->duration=(runner->config->synth->rate*RB_BEEP_DURATION_MS)/1000;
  if (RUNNER->duration<100) RUNNER->duration=100;
  
  RUNNER->dc=(runner->config->synth->rate*RB_BEEP_PEAK_TIME_MS)/1000;
  if (RUNNER->dc<1) RUNNER->dc=1;
  RUNNER->dlevel=RCONFIG->level/RUNNER->dc;
  
  rb_sample_t rate=rb_rate_from_noteid(noteid);
  RUNNER->dp=(rate*2.0f)/runner->config->synth->rate; // 2 not 1; it's actually a half-period
  RUNNER->p=0.0f;
  
  runner->update=_rb_beep_runner_update;
  return 0;
}

/* Get duration.
 */
 
static int _rb_beep_runner_get_duration(struct rb_synth_node_runner *runner) {
  return RUNNER->duration;
}

/* Type definition.
 */
 
static const struct rb_synth_node_field _rb_beep_fieldv[]={
  {
    .fldid=RB_BEEP_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_beep*)0)->mainv,
  },
  {
    .fldid=RB_BEEP_FLDID_level,
    .name="level",
    .desc="Output level, 0..1. Default 0.25.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_beep*)0)->level,
  },
};

const struct rb_synth_node_type rb_synth_node_type_beep={
  .ntid=RB_SYNTH_NTID_beep,
  .name="beep",
  .desc="Trivial square wave instrument, entirely self-contained.",
  .config_objlen=sizeof(struct rb_synth_node_config_beep),
  .runner_objlen=sizeof(struct rb_synth_node_runner_beep),
  .fieldv=_rb_beep_fieldv,
  .fieldc=sizeof(_rb_beep_fieldv)/sizeof(struct rb_synth_node_field),
  .config_init=_rb_beep_config_init,
  .config_ready=_rb_beep_config_ready,
  .runner_init=_rb_beep_runner_init,
  .runner_get_duration=_rb_beep_runner_get_duration,
};
