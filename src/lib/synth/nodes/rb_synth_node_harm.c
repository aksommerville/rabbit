#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"
#include <math.h>

#define RB_HARM_FLDID_main 0x01
#define RB_HARM_FLDID_rate 0x02
#define RB_HARM_FLDID_phase 0x03
#define RB_HARM_FLDID_coefv 0x04

#define RB_HARM_COEF_LIMIT 16
#define RB_HARM_SERIAL_LIMIT (RB_HARM_COEF_LIMIT*2)

/* Instance definition.
 */
 
struct rb_synth_node_config_harm {
  struct rb_synth_node_config hdr;
  rb_sample_t rate; // hz
  rb_sample_t phase; // 0..1
  rb_sample_t coefv[RB_HARM_COEF_LIMIT];
  uint8_t serial[RB_HARM_SERIAL_LIMIT];
  int coefc;
};

struct rb_synth_node_runner_harm {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  rb_sample_t *ratev; // hz
  rb_sample_t *phasev; // 0..1
  rb_sample_t rate; // dp per frame
  rb_sample_t p; // 0..2pi; position of fundamental, and the others are trivially derived
};

#define CONFIG ((struct rb_synth_node_config_harm*)config)
#define RUNNER ((struct rb_synth_node_runner_harm*)runner)
#define RCONFIG ((struct rb_synth_node_config_harm*)(runner->config))

/* Init config.
 */
 
static int _rb_harm_config_init(struct rb_synth_node_config *config) {
  CONFIG->rate=1.0f;
  CONFIG->phase=0.0f;
  CONFIG->serial[0]=0xff;
  return 0;
}

/* Ready config.
 */
 
static int _rb_harm_config_ready(struct rb_synth_node_config *config) {

  //TODO Optional 16-bit coefficients?
  //TODO Optional 1/n normalization?
  //TODO Master level?
  
  // Count coefficients in serial. Can be zero.
  CONFIG->coefc=RB_HARM_COEF_LIMIT;
  while (CONFIG->coefc&&!CONFIG->serial[CONFIG->coefc-1]) CONFIG->coefc--;
  
  // Expand each coefficient to floating-point.
  int i=CONFIG->coefc;
  rb_sample_t *dst=CONFIG->coefv;
  const uint8_t *src=CONFIG->serial;
  for (;i-->0;dst++,src++) {
    *dst=(*src)/255.0f;
  }

  return 0;
}

/* Update.
 */
 
static void _rb_harm_update_silent(struct rb_synth_node_runner *runner,int c) {
  memset(RUNNER->mainv,0,sizeof(rb_sample_t)*c);
}
 
static void _rb_harm_update_phasev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t scale=M_PI*2.0f;
  rb_sample_t *dst=RUNNER->mainv;
  const rb_sample_t *phase=RUNNER->phasev;
  for (;c-->0;dst++,phase++) {
    *dst=0.0f;
    
    rb_sample_t phase1=(*phase)*scale;
    rb_sample_t p=phase1;
    const rb_sample_t *coef=RCONFIG->coefv;
    int i=RCONFIG->coefc;
    for (;i-->0;coef++,p+=phase1) {
      if (*coef>0.0f) {
        (*dst)+=sinf(p)*(*coef); // SAMPLETYPE
      }
    }
  }
}
 
static void _rb_harm_update_ratev(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t scale=(M_PI*2.0f)/runner->config->synth->rate;
  rb_sample_t *dst=RUNNER->mainv;
  const rb_sample_t *rate=RUNNER->ratev;
  for (;c-->0;dst++,rate++) {
    *dst=0.0f;
    
    rb_sample_t p=RUNNER->p;
    const rb_sample_t *coef=RCONFIG->coefv;
    int i=RCONFIG->coefc;
    for (;i-->0;coef++,p+=RUNNER->p) {
      if (*coef>0.0f) {
        (*dst)+=sinf(p)*(*coef); // SAMPLETYPE
      }
    }
    
    RUNNER->p+=(*rate)*scale;
    if (RUNNER->p>=M_PI*2.0f) RUNNER->p-=M_PI*2.0f;
    else if (RUNNER->p<0.0f) RUNNER->p+=M_PI*2.0f;
  }
}
 
static void _rb_harm_update_fixed(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *dst=RUNNER->mainv;
  for (;c-->0;dst++) {
    *dst=0.0f;
    
    rb_sample_t p=RUNNER->p;
    const rb_sample_t *coef=RCONFIG->coefv;
    int i=RCONFIG->coefc;
    for (;i-->0;coef++,p+=RUNNER->p) {
      if (*coef>0.0f) {
        (*dst)+=sinf(p)*(*coef); // SAMPLETYPE
      }
    }
    
    RUNNER->p+=RUNNER->rate;
    if (RUNNER->p>=M_PI*2.0f) RUNNER->p-=M_PI*2.0f;
  }
}

/* Init runner.
 */
 
static int _rb_harm_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  
  if (rb_synth_node_config_find_link(runner->config,RB_HARM_FLDID_rate)<0) {
    RUNNER->rate=RCONFIG->rate;
  }
  RUNNER->rate=(RUNNER->rate*M_PI*2.0f)/runner->config->synth->rate;
  
  RUNNER->p=RCONFIG->phase*M_PI*2.0f;
  
  if (!RCONFIG->coefc) runner->update=_rb_harm_update_silent;
  else if (RUNNER->phasev) runner->update=_rb_harm_update_phasev;
  else if (RUNNER->ratev) runner->update=_rb_harm_update_ratev;
  else runner->update=_rb_harm_update_fixed;
  return 0;
}

/* Set fields.
 */
 
static int _rb_harm_set_coefv(struct rb_synth_node_config *config,const void *src,int srcc) {
  if (srcc>RB_HARM_SERIAL_LIMIT) srcc=RB_HARM_SERIAL_LIMIT;
  memcpy(CONFIG->serial,src,srcc);
  if (srcc<RB_HARM_SERIAL_LIMIT) memset(CONFIG->serial+srcc,0,RB_HARM_SERIAL_LIMIT-srcc);
  return 0;
}

/* Fields.
 */
 
static const struct rb_synth_node_field _rb_harm_fieldv[]={
  {
    .fldid=RB_HARM_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED|RB_SYNTH_NODE_FIELD_BUF0IFNONE,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_harm*)0)->mainv,
  },
  {
    .fldid=RB_HARM_FLDID_rate,
    .name="rate",
    .desc="Playback rate in Hz, normally RB_SYNTH_LINK_NOTEHZ.",
    // Technically not required; user may set (phase) instead.
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_harm*)0)->rate,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_harm*)0)->ratev,
    .runner_offsetf=(uintptr_t)&((struct rb_synth_node_runner_harm*)0)->rate,
  },
  {
    .fldid=RB_HARM_FLDID_phase,
    .name="phase",
    .desc="Initial phase 0..1, or assign from a vector to play any crazy path thru the wave.",
    .config_offsetf=(uintptr_t)&((struct rb_synth_node_config_harm*)0)->phase,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_harm*)0)->phasev,
  },
  {
    .fldid=RB_HARM_FLDID_coefv,
    .name="coefv",
    .desc="Coefficients",
    .flags=RB_SYNTH_NODE_FIELD_PRINCIPAL,
    .serialfmt=RB_SYNTH_SERIALFMT_COEFV,
    .config_sets=_rb_harm_set_coefv,
  },
};

/* Type definition.
 */
 
const struct rb_synth_node_type rb_synth_node_type_harm={
  .ntid=RB_SYNTH_NTID_harm,
  .name="harm",
  .desc="Oscillator with a wave formed by a small set of harmonics.",
  .flags=0,
  .config_objlen=sizeof(struct rb_synth_node_config_harm),
  .runner_objlen=sizeof(struct rb_synth_node_runner_harm),
  .fieldv=_rb_harm_fieldv,
  .fieldc=sizeof(_rb_harm_fieldv)/sizeof(struct rb_synth_node_field),
  .config_init=_rb_harm_config_init,
  .config_ready=_rb_harm_config_ready,
  .runner_init=_rb_harm_runner_init,
};
