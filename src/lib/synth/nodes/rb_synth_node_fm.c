#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"
#include <math.h>

#define RB_FM_FLDID_main  1
#define RB_FM_FLDID_rate  2
#define RB_FM_FLDID_mod0  3
#define RB_FM_FLDID_mod1  4
#define RB_FM_FLDID_range 5

/* Instance definition.
 */
 
struct rb_synth_node_config_fm {
  struct rb_synth_node_config hdr;
  rb_sample_t rate; // configured in hertz; converted to 0..2pi at ready
  rb_sample_t mod0; // hertz=>0..2pi like rate
  rb_sample_t mod1; // relative
  rb_sample_t range; // relative
};

struct rb_synth_node_runner_fm {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  rb_sample_t ratek;
  rb_sample_t *ratev;
  rb_sample_t *rangev;
  rb_sample_t carp;
  rb_sample_t modp;
};

#define CONFIG ((struct rb_synth_node_config_fm*)config)
#define RUNNER ((struct rb_synth_node_runner_fm*)runner)
#define RCONFIG ((struct rb_synth_node_config_fm*)runner->config)

/* Cleanup.
 */
 
static void _rb_fm_config_del(struct rb_synth_node_config *config) {
}

static void _rb_fm_runner_del(struct rb_synth_node_runner *runner) {
}

/* Config init.
 */
 
static int _rb_fm_config_init(struct rb_synth_node_config *config) {
  return 0;
}

/* Config ready.
 */
 
static int _rb_fm_config_ready(struct rb_synth_node_config *config) {

  CONFIG->rate=(CONFIG->rate*M_PI*2.0f)/config->synth->rate;
  CONFIG->rate=fmodf(CONFIG->rate,M_PI*2.0f); // SAMPLETYPE
  if (CONFIG->rate<0.0f) CONFIG->rate+=M_PI*2.0f;

  CONFIG->mod0=(CONFIG->mod0*M_PI*2.0f)/config->synth->rate;
  CONFIG->mod0=fmodf(CONFIG->mod0,M_PI*2.0f); // SAMPLETYPE
  if (CONFIG->mod0<0.0f) CONFIG->mod0+=M_PI*2.0f;

  return 0;
}

/* Update.
 */
 
static void _rb_fm_update_universal(struct rb_synth_node_runner *runner,int c) {
  //TODO Just getting the algorithm right... Make a copy of this for each vector-vs-scalar case.
  // ...it's *crazy* to do all these null checks every single frame!
  rb_sample_t *main=RUNNER->mainv;
  rb_sample_t *rate=RUNNER->ratev;
  rb_sample_t *range=RUNNER->rangev;
  for (;c-->0;main++) {
  
    *main=sinf(RUNNER->carp); // SAMPLETYPE
    
    rb_sample_t cardp=RUNNER->ratek;
    if (rate) {
      cardp=((*rate)*M_PI*2.0f)/runner->config->synth->rate;
      if (cardp<0.0f) cardp=0.0f;
      else if (cardp>M_PI*2.0f) cardp=0.0f;
    }
    
    rb_sample_t mod=sinf(RUNNER->modp); // SAMPLETYPE
    rb_sample_t moddp=RCONFIG->mod0+cardp*RCONFIG->mod1;
    RUNNER->modp+=moddp;
    if (RUNNER->modp>=M_PI*2.0f) RUNNER->modp-=M_PI*2.0f;
    if (range) {
      mod*=(*range);
    } else {
      mod*=RCONFIG->range;
    }
    
    cardp=cardp+cardp*mod;
    RUNNER->carp+=cardp;
    if (RUNNER->carp>=M_PI*2.0f) RUNNER->carp-=M_PI*2.0f;
    else if (RUNNER->carp<0.0f) RUNNER->carp+=M_PI*2.0f;
  
    if (rate) rate++;
    if (range) range++;
  }
}

/* Runner init.
 */
 
static int _rb_fm_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  
  //TODO permit config-assigned rate
  RUNNER->ratek=(RUNNER->ratek*M_PI*2.0f)/runner->config->synth->rate;
  RUNNER->ratek=fmodf(RUNNER->ratek,M_PI*2.0f); // SAMPLETYPE
  if (RUNNER->ratek<0.0f) RUNNER->ratek+=M_PI*2.0f;
  
  //TODO optimized update cases
  runner->update=_rb_fm_update_universal;
  return 0;
}

/* Type definition.
 */
 
static const struct rb_synth_node_field _rb_fm_fieldv[]={
  {
    .fldid=RB_FM_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_fm*)0)->mainv,
  },
  {
    .fldid=RB_FM_FLDID_rate,
    .name="rate",
    .desc="Carrier rate in Hertz.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_fm*)0)->rate,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_fm*)0)->ratev,
    .runner_offsetf=(uintptr_t)&((struct rb_synth_node_runner_fm*)0)->ratek,
  },
  {
    .fldid=RB_FM_FLDID_mod0,
    .name="mod0",
    .desc="Modulator rate in Hertz (absolute component).",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_fm*)0)->mod0,
  },
  {
    .fldid=RB_FM_FLDID_mod1,
    .name="mod1",
    .desc="Modulator rate relative to carrier rate (+mod0).",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_fm*)0)->mod1,
  },
  {
    .fldid=RB_FM_FLDID_range,
    .name="range",
    .desc="Amount of modulation. 0=none, 6 or 7 is pushing it.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_fm*)0)->range,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_fm*)0)->rangev,
  },
};

const struct rb_synth_node_type rb_synth_node_type_fm={
  .name="fm",
  .desc="General-purpose FM synthesizer.",
  .config_objlen=sizeof(struct rb_synth_node_config_fm),
  .runner_objlen=sizeof(struct rb_synth_node_runner_fm),
  .fieldv=_rb_fm_fieldv,
  .fieldc=sizeof(_rb_fm_fieldv)/sizeof(struct rb_synth_node_field),
  .config_del=_rb_fm_config_del,
  .runner_del=_rb_fm_runner_del,
  .config_init=_rb_fm_config_init,
  .config_ready=_rb_fm_config_ready,
  .runner_init=_rb_fm_runner_init,
};
