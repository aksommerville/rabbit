#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"
#include <math.h>

#define RB_OSC_FLDID_main 0x01
#define RB_OSC_FLDID_rate 0x02
#define RB_OSC_FLDID_shape 0x03
#define RB_OSC_FLDID_phase 0x04
#define RB_OSC_FLDID_level 0x05

/* Instance definition.
 */
 
struct rb_synth_node_config_osc {
  struct rb_synth_node_config hdr;
  rb_sample_t rate;
  int shape;
  rb_sample_t phase;
  rb_sample_t level;
  rb_sample_t invrate;
};

struct rb_synth_node_runner_osc {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  rb_sample_t *ratev;
  rb_sample_t *phasev;
  rb_sample_t rate;
  rb_sample_t p;
  rb_sample_t dp;
  rb_sample_t k; // for update hook's use, no fixed meaning
};

#define CONFIG ((struct rb_synth_node_config_osc*)config)
#define RUNNER ((struct rb_synth_node_runner_osc*)runner)
#define RCONFIG ((struct rb_synth_node_config_osc*)(runner->config))

/* Init config.
 */
 
static int _rb_osc_config_init(struct rb_synth_node_config *config) {
  CONFIG->rate=1.0f;
  CONFIG->shape=RB_OSC_SHAPE_SINE;
  CONFIG->phase=0.0f;
  CONFIG->level=1.0f;
  CONFIG->invrate=1.0f/config->synth->rate;
  return 0;
}

/* Ready config.
 */
 
static int _rb_osc_config_ready(struct rb_synth_node_config *config) {
  return 0;
}

/* SINE
 */
 
static void _rb_osc_update_sine_phasev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t level=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *phase=RUNNER->phasev;
  for (;c-->0;v++,phase++) {
    *v=sinf((*phase)*M_PI*2.0f)*level; // SAMPLETYPE
  }
}
 
static void _rb_osc_update_sine_ratev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t level=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *rate=RUNNER->ratev;
  for (;c-->0;v++,rate++) {
    *v=sinf(RUNNER->p)*level; // SAMPLETYPE
    RUNNER->p+=(*rate)*RUNNER->k;
    if (RUNNER->p>=M_PI) RUNNER->p-=M_PI*2.0f;
  }
}
 
static void _rb_osc_update_sine_const(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t level=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  for (;c-->0;v++) {
    *v=sinf(RUNNER->p)*level; // SAMPLETYPE
    RUNNER->p+=RUNNER->dp;
    if (RUNNER->p>=M_PI) RUNNER->p-=M_PI*2.0f;
  }
}

/* SQUARE
 */
 
static void _rb_osc_update_square_phasev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t level=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *phase=RUNNER->phasev;
  for (;c-->0;v++,phase++) {
    if (*phase>=0.5f) *v=-level;
    else *v=level;
  }
}
 
static void _rb_osc_update_square_ratev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t level=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *rate=RUNNER->ratev;
  for (;c-->0;v++,rate++) {
    if (RUNNER->p>=0.5f) *v=-level;
    else *v=level;
    RUNNER->p+=(*rate)*RCONFIG->invrate;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
  }
}
 
static void _rb_osc_update_square_const(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t level=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  for (;c-->0;v++) {
    //TODO Get smarter about this, use a single rb_signal_set_s() for each half-period.
    if (RUNNER->p>=0.5f) *v=-level;
    else *v=level;
    RUNNER->p+=RUNNER->dp;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
  }
}

/* SAWUP
 */
 
static void _rb_osc_update_sawup_phasev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t base=-RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *phase=RUNNER->phasev;
  for (;c-->0;v++,phase++) {
    *v=base+(*phase)*RUNNER->k;
  }
}
 
static void _rb_osc_update_sawup_ratev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t base=-RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *rate=RUNNER->ratev;
  for (;c-->0;v++,rate++) {
    *v=base+RUNNER->p*RUNNER->k;
    RUNNER->p+=(*rate)*RCONFIG->invrate;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
  }
}
 
static void _rb_osc_update_sawup_const(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t base=-RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  for (;c-->0;v++) {
    *v=base+RUNNER->p*RUNNER->k;
    RUNNER->p+=RUNNER->dp;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
  }
}

/* SAWDOWN
 */
 
static void _rb_osc_update_sawdown_phasev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t base=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *phase=RUNNER->phasev;
  for (;c-->0;v++,phase++) {
    *v=base+(*phase)*RUNNER->k;
  }
}
 
static void _rb_osc_update_sawdown_ratev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t base=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *rate=RUNNER->ratev;
  for (;c-->0;v++,rate++) {
    *v=base+RUNNER->p*RUNNER->k;
    RUNNER->p+=(*rate)*RCONFIG->invrate;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
  }
}
 
static void _rb_osc_update_sawdown_const(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t base=RCONFIG->level;
  rb_sample_t *v=RUNNER->mainv;
  for (;c-->0;v++) {
    *v=base+RUNNER->p*RUNNER->k;
    RUNNER->p+=RUNNER->dp;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
  }
}

/* TRIANGLE
 */
 
static void _rb_osc_update_triangle_phasev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *phase=RUNNER->phasev;
  for (;c-->0;v++,phase++) {
    if (*phase>=0.5f) *v=RCONFIG->level-((*phase)-0.5f)*RUNNER->k;
    else *v=(*phase)*RUNNER->k-RCONFIG->level;
  }
}
 
static void _rb_osc_update_triangle_ratev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *rate=RUNNER->ratev;
  for (;c-->0;v++,rate++) {
    if (RUNNER->p>=0.5f) *v=RCONFIG->level-(RUNNER->p-0.5f)*RUNNER->k;
    else *v=RUNNER->p*RUNNER->k-RCONFIG->level;
    RUNNER->p+=(*rate)*RCONFIG->invrate;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
    else if (RUNNER->p<0.0f) RUNNER->p+=1.0f;
  }
}
 
static void _rb_osc_update_triangle_const(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *v=RUNNER->mainv;
  for (;c-->0;v++) {
    if (RUNNER->p>=0.5f) *v=RCONFIG->level-(RUNNER->p-0.5f)*RUNNER->k;
    else *v=RUNNER->p*RUNNER->k-RCONFIG->level;
    RUNNER->p+=RUNNER->dp;
    if (RUNNER->p>=1.0f) RUNNER->p-=1.0f;
  }
}

/* IMPULSE
 */
 
static void _rb_osc_update_impulse_phasev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *phase=RUNNER->phasev;
  for (;c-->0;v++,phase++) {
    if ((*phase)<RUNNER->p) *v=RCONFIG->level;
    else *v=0.0f;
    RUNNER->p=*phase;
  }
}
 
static void _rb_osc_update_impulse_ratev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *v=RUNNER->mainv;
  const rb_sample_t *rate=RUNNER->ratev;
  for (;c-->0;v++,rate++) {
    if (RUNNER->p>=1.0f) {
      *v=RCONFIG->level;
      RUNNER->p-=1.0f;
    } else {
      *v=0.0f;
    }
    RUNNER->p+=(*rate)*RCONFIG->invrate;
  }
}
 
static void _rb_osc_update_impulse_const(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *v=RUNNER->mainv;
  for (;c-->0;v++) {
    if (RUNNER->p>=1.0f) {
      *v=RCONFIG->level;
      RUNNER->p-=1.0f;
    } else {
      *v=0.0f;
    }
    RUNNER->p+=RUNNER->dp;
  }
}

/* NOISE
 */
 
static void _rb_osc_update_noise(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *v=RUNNER->mainv;
  for (;c-->0;v++) {
    *v=((rand()&0xffff)-32768)*RUNNER->k;
  }
}

/* DC
 */
 
static void _rb_osc_update_dc(struct rb_synth_node_runner *runner,int c) {
  rb_signal_set_s(RUNNER->mainv,c,RCONFIG->level);
}

/* Init runner.
 */
 
static int _rb_osc_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  
  if (rb_synth_node_config_find_link(runner->config,RB_OSC_FLDID_rate)<0) {
    RUNNER->rate=RCONFIG->rate;
  }
  
  // Set (p,dp) based on a 0..1 range, the most common.
  // These will be ignored if (phasev) or (ratev) was set, whatever.
  rb_sample_t dummy;
  RUNNER->p=modff(RCONFIG->phase,&dummy); // SAMPLETYPE
  RUNNER->dp=RUNNER->rate/runner->config->synth->rate;
  
  // Tons of (update) hooks, each dialed in to a very specific setup.
  switch (RCONFIG->shape) {
    case RB_OSC_SHAPE_SINE: {
        RUNNER->p*=M_PI*2.0f;
        RUNNER->dp*=M_PI*2.0f;
        RUNNER->k=(M_PI*2.0f)/runner->config->synth->rate;
        if (RUNNER->phasev) runner->update=_rb_osc_update_sine_phasev;
        else if (RUNNER->ratev) runner->update=_rb_osc_update_sine_ratev;
        else runner->update=_rb_osc_update_sine_const;
      } break;
    case RB_OSC_SHAPE_SQUARE: {
        if (RUNNER->phasev) runner->update=_rb_osc_update_square_phasev;
        else if (RUNNER->ratev) runner->update=_rb_osc_update_square_ratev;
        else runner->update=_rb_osc_update_square_const;
      } break;
    case RB_OSC_SHAPE_SAWUP: {
        RUNNER->k=RCONFIG->level*2.0f;
        if (RUNNER->phasev) runner->update=_rb_osc_update_sawup_phasev;
        else if (RUNNER->ratev) runner->update=_rb_osc_update_sawup_ratev;
        else runner->update=_rb_osc_update_sawup_const;
      } break;
    case RB_OSC_SHAPE_SAWDOWN: {
        RUNNER->k=RCONFIG->level*-2.0f;
        if (RUNNER->phasev) runner->update=_rb_osc_update_sawdown_phasev;
        else if (RUNNER->ratev) runner->update=_rb_osc_update_sawdown_ratev;
        else runner->update=_rb_osc_update_sawdown_const;
      } break;
    case RB_OSC_SHAPE_TRIANGLE: {
        RUNNER->k=RCONFIG->level*4.0f;
        if (RUNNER->phasev) runner->update=_rb_osc_update_triangle_phasev;
        else if (RUNNER->ratev) runner->update=_rb_osc_update_triangle_ratev;
        else runner->update=_rb_osc_update_triangle_const;
      } break;
    case RB_OSC_SHAPE_IMPULSE: {
        if (RUNNER->p<=0.0f) RUNNER->p=1.0f; // normal zero-phase case, DO emit an impulse on the very first frame
        if (RUNNER->phasev) runner->update=_rb_osc_update_impulse_phasev;
        else if (RUNNER->ratev) runner->update=_rb_osc_update_impulse_ratev;
        else runner->update=_rb_osc_update_impulse_const;
      } break;
    case RB_OSC_SHAPE_NOISE: {
        RUNNER->k=RCONFIG->level/32768.0f;
        runner->update=_rb_osc_update_noise;
      } break;
    case RB_OSC_SHAPE_DC: runner->update=_rb_osc_update_dc; break;
    default: return -1;
  }
  return 0;
}

/* Fields.
 */
 
static const struct rb_synth_node_field _rb_osc_fieldv[]={
  {
    .fldid=RB_OSC_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED|RB_SYNTH_NODE_FIELD_BUF0IFNONE,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_osc*)0)->mainv,
  },
  {
    .fldid=RB_OSC_FLDID_rate,
    .name="rate",
    .desc="Playback rate in Hz, normally RB_SYNTH_LINK_NOTEHZ.",
    // Technically not required; user may set (phase) instead.
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_osc*)0)->rate,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_osc*)0)->ratev,
    .runner_offsetf=(uintptr_t)&((struct rb_synth_node_runner_osc*)0)->rate,
  },
  {
    .fldid=RB_OSC_FLDID_shape,
    .name="shape",
    .desc="What kind of wave? Default sine.",
    .enumlabels="sine,square,sawup,sawdown,triangle,impulse,noise,dc",
    .config_offseti=(uintptr_t)&((struct rb_synth_node_config_osc*)0)->shape,
  },
  {
    .fldid=RB_OSC_FLDID_phase,
    .name="phase",
    .desc="Initial phase 0..1, or assign from a vector to play any crazy path thru the wave.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_osc*)0)->phase,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_osc*)0)->phasev,
  },
  {
    .fldid=RB_OSC_FLDID_level,
    .name="level",
    .desc="Peak output level, default 1.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_osc*)0)->level,
  },
};

/* Type definition.
 */
 
const struct rb_synth_node_type rb_synth_node_type_osc={
  .ntid=RB_SYNTH_NTID_osc,
  .name="osc",
  .desc="Oscillator",
  .flags=0,
  .config_objlen=sizeof(struct rb_synth_node_config_osc),
  .runner_objlen=sizeof(struct rb_synth_node_runner_osc),
  .fieldv=_rb_osc_fieldv,
  .fieldc=sizeof(_rb_osc_fieldv)/sizeof(struct rb_synth_node_field),
  .config_init=_rb_osc_config_init,
  .config_ready=_rb_osc_config_ready,
  .runner_init=_rb_osc_runner_init,
};
