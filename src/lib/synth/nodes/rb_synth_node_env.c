#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"
#include <math.h>

#define RB_ENV_FLDID_main 0x01
#define RB_ENV_FLDID_mode 0x02
#define RB_ENV_FLDID_content 0x03

static int _rb_env_set_content(struct rb_synth_node_config *config,const void *src,int srcc);

/* Instance definition.
 */
 
struct rb_synth_node_config_env {
  struct rb_synth_node_config hdr;
  int mode;
  int seriali; // weird hack to allow setting preset format as integer
  rb_sample_t level0;
  struct rb_env_point {
    int time; // frames
    rb_sample_t level; // final level
    rb_sample_t curve; // -1..1
    // Derived:
    int iscurve;
    rb_sample_t dlevel; // linear delta
    rb_sample_t levelm; // exponential state multiplier
    rb_sample_t levelk; // exponential scale multiplier
  } *pointv;
  int pointc,pointa;
};

struct rb_synth_node_runner_env {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  int pointp;
  struct rb_env_point *point; // RCONFIG->pointv+pointp; or null if finished
  int time; // counts down to end of point
  rb_sample_t level;
  rb_sample_t levelf; // exponential state
  rb_sample_t legbase;
};

#define CONFIG ((struct rb_synth_node_config_env*)config)
#define RUNNER ((struct rb_synth_node_runner_env*)runner)
#define RCONFIG ((struct rb_synth_node_config_env*)(runner->config))

/* Cleanup.
 */
 
static void _rb_env_config_del(struct rb_synth_node_config *config) {
  if (CONFIG->pointv) free(CONFIG->pointv);
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

  if (!CONFIG->pointc) {
    if ((CONFIG->seriali>=0x80)&&(CONFIG->seriali<=0xff)) {
      uint8_t serial=CONFIG->seriali;
      if (_rb_env_set_content(config,&serial,1)<0) return -1;
    } else {
      return rb_synth_error(config->synth,"env.content required");
    }
  }

  return 0;
}

/* Update.
 */
 
#define RB_ENV_UPDATE_COMMON(apply) \
  rb_sample_t *v=RUNNER->mainv; \
  for (;c-->0;v++) { \
   \
    if (RUNNER->time<=0) { \
      RUNNER->pointp++; \
      if (RUNNER->pointp>=RCONFIG->pointc) { \
        RUNNER->pointp=RCONFIG->pointc; \
        RUNNER->point=0; \
        if ((RCONFIG->mode==0)||(RCONFIG->mode==1)) { \
          /* mlt or set, zero the remainder. add, do nothing */ \
          memset(v,0,sizeof(rb_sample_t)*(c+1)); \
        } \
        return; \
      } \
      RUNNER->level=RUNNER->point->level; \
      RUNNER->point++; \
      RUNNER->time=RUNNER->point->time; \
      if (RUNNER->point->levelm>1.0f) RUNNER->levelf=RUNNER->point->levelm; \
      else RUNNER->levelf=RUNNER->point->levelk; \
      RUNNER->legbase=RUNNER->level; \
    } \
    \
    apply; \
    \
    RUNNER->time--; \
    if (RUNNER->point->iscurve>0) { \
      RUNNER->levelf*=RUNNER->point->levelm; \
      RUNNER->level=RUNNER->point->level+(RUNNER->levelf-RUNNER->point->levelm)*RUNNER->point->dlevel; \
    } else if (RUNNER->point->iscurve<0) { \
      RUNNER->levelf*=RUNNER->point->levelm; \
      RUNNER->level=(RUNNER->levelf-RUNNER->point->levelm)*RUNNER->point->dlevel+RUNNER->legbase; \
    } else { \
      RUNNER->level+=RUNNER->point->dlevel; \
    } \
  }
 
static void _rb_env_update_mlt(struct rb_synth_node_runner *runner,int c) {
  RB_ENV_UPDATE_COMMON((*v)*=RUNNER->level)
}

static void _rb_env_update_set(struct rb_synth_node_runner *runner,int c) {
  RB_ENV_UPDATE_COMMON((*v)=RUNNER->level)
}

static void _rb_env_update_add(struct rb_synth_node_runner *runner,int c) {
  RB_ENV_UPDATE_COMMON((*v)+=RUNNER->level)
}

#undef RB_ENV_UPDATE_COMMON

/* Init runner.
 */
 
static int _rb_env_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  
  RUNNER->level=RCONFIG->level0;
  RUNNER->point=RCONFIG->pointv;
  if (RUNNER->point->levelm>1.0f) RUNNER->levelf=RUNNER->point->levelm;
  else RUNNER->levelf=RUNNER->point->levelk;
  RUNNER->legbase=RUNNER->level;
  RUNNER->time=RUNNER->point->time;
  
  switch (RCONFIG->mode) {
    case 0: runner->update=_rb_env_update_mlt; break;
    case 1: runner->update=_rb_env_update_set; break;
    case 2: runner->update=_rb_env_update_add; break;
    default: return -1;
  }
  return 0;
}

/* Get duration.
 */
 
static int _rb_env_runner_get_duration(struct rb_synth_node_runner *runner) {
  int total=0,i=RCONFIG->pointc;
  const struct rb_env_point *point=RCONFIG->pointv;
  for (;i-->0;point++) total+=point->time;
  return total;
}

/* Digest points after decoding.
 */
 
static void rb_env_digest_points(struct rb_synth_node_config *config) {
  const rb_sample_t RB_ENV_CURVE_CONSTANT=100.0f;
  rb_sample_t inlevel=CONFIG->level0;
  struct rb_env_point *point=CONFIG->pointv;
  int i=CONFIG->pointc;
  for (;i-->0;point++) {
    if (point->time<1) point->time=1;
    if (point->curve<-0.001f) {
      point->iscurve=-1;
      point->levelk=1.0f-point->curve*RB_ENV_CURVE_CONSTANT;
      point->levelm=powf(point->levelk,1.0f/point->time); // SAMPLETYPE
      point->dlevel=(point->level-inlevel)/(point->levelk-point->levelm);
      
    } else if (point->curve>0.001f) {
      point->iscurve=1;
      point->levelk=1.0f+point->curve*RB_ENV_CURVE_CONSTANT;
      point->levelm=1.0f/powf(point->levelk,1.0f/point->time); // SAMPLETYPE
      point->dlevel=(inlevel-point->level)/(point->levelk-point->levelm);
    
    } else {
      point->iscurve=0;
      point->dlevel=(point->level-inlevel)/point->time;
    }
    inlevel=point->level;
  }
}

/* Decode and scale scalars for long format.
 */
 
static int rb_env_decode_scalar_u8(rb_sample_t *dst,const uint8_t *src,int srcc,rb_sample_t range) {
  if (srcc<1) return -1;
  *dst=(src[0]*range)/255.0f;
  return 1;
}
 
static int rb_env_decode_scalar_s8(rb_sample_t *dst,const uint8_t *src,int srcc,rb_sample_t range) {
  if (srcc<1) return -1;
  *dst=(((int8_t)src[0])*range)/255.0f;
  return 1;
}
 
static int rb_env_decode_scalar_u16(rb_sample_t *dst,const uint8_t *src,int srcc,rb_sample_t range) {
  if (srcc<2) return -1;
  uint16_t i=(src[0]<<8)|src[1];
  *dst=(i*range)/65535.0f;
  return 2;
}
 
static int rb_env_decode_scalar_s16(rb_sample_t *dst,const uint8_t *src,int srcc,rb_sample_t range) {
  if (srcc<2) return -1;
  int16_t i=(src[0]<<8)|src[1];
  *dst=(i*range)/32768.0f;
  return 2;
}

/* Decode long format.
 */
 
static int rb_env_decode_long(struct rb_synth_node_config *config,const uint8_t *src,int srcc) {
  int srcp=0,err;
  
  uint8_t flags=src[srcp++];
  
  rb_sample_t timerange=1.0f;
  if (flags&RB_ENV_FLAG_TIME_RANGE) {
    if (srcp>=srcc) return -1;
    timerange=src[srcp++]/64.0f;
  }
  
  rb_sample_t levelrange=1.0f;
  if (flags&RB_ENV_FLAG_LEVEL_RANGE) {
    if (srcp>srcc-3) return -1;
    levelrange=((src[srcp]<<16)|(src[srcp+1]<<8)|src[srcp+2])/256.0f;
    srcp+=3;
  }
  
  int (*rdtime)(rb_sample_t *dst,const uint8_t *src,int srcc,rb_sample_t range);
  int (*rdlevel)(rb_sample_t *dst,const uint8_t *src,int srcc,rb_sample_t range);
  fprintf(stderr,"flags=%02x HIRES_TIME=%02x\n",flags,RB_ENV_FLAG_HIRES_TIME);
  if (flags&RB_ENV_FLAG_HIRES_TIME) rdtime=rb_env_decode_scalar_u16;
  else rdtime=rb_env_decode_scalar_u8;
  if (flags&RB_ENV_FLAG_SIGNED_LEVEL) {
    if (flags&RB_ENV_FLAG_HIRES_LEVEL) rdlevel=rb_env_decode_scalar_s16;
    else rdlevel=rb_env_decode_scalar_s8;
  } else {
    if (flags&RB_ENV_FLAG_HIRES_LEVEL) rdlevel=rb_env_decode_scalar_u16;
    else rdlevel=rb_env_decode_scalar_u8;
  }
  
  if (flags&RB_ENV_FLAG_INIT_LEVEL) {
    if ((err=rdlevel(&CONFIG->level0,src+srcp,srcc-srcp,levelrange))<1) return -1;
    srcp+=err;
  }
  
  while (srcp<srcc) {
    if (CONFIG->pointc>=CONFIG->pointa) {
      int na=CONFIG->pointa+4;
      if (na>INT_MAX/sizeof(struct rb_env_point)) return -1;
      void *nv=realloc(CONFIG->pointv,sizeof(struct rb_env_point)*na);
      if (!nv) return -1;
      CONFIG->pointv=nv;
      CONFIG->pointa=na;
    }
    
    // If time is exactly zero, consume it and terminate
    if (flags&RB_ENV_FLAG_HIRES_TIME) {
      if (srcp>srcc-2) return -1;
      if (!src[srcp]&&!src[srcp+1]) {
        srcp+=2;
        break;
      }
    } else {
      if (!src[srcp]) {
        srcp++;
        break;
      }
    }
    
    struct rb_env_point *point=CONFIG->pointv+CONFIG->pointc++;
    rb_sample_t f;
    if ((err=rdtime(&f,src+srcp,srcc-srcp,timerange))<1) return -1;
    srcp+=err;
    point->time=f*config->synth->rate;
    if ((err=rdlevel(&point->level,src+srcp,srcc-srcp,levelrange))<1) return -1;
    srcp+=err;
    if (flags&RB_ENV_FLAG_CURVE) {
      if ((err=rb_env_decode_scalar_s8(&point->curve,src+srcp,srcc-srcp,1.0f))<1) return -1;
      srcp+=err;
    }
  }

  if (!CONFIG->pointc) return rb_synth_error(config->synth,
    "At least one point is required in env.content"
  );
  rb_env_digest_points(config);
  return 0;
}

/* Set content.
 */
 
static int _rb_env_set_content(struct rb_synth_node_config *config,const void *src,int srcc) {
  const uint8_t *SRC=src;
  if (CONFIG->pointc) return -1;
  if (srcc<1) return -1;
  
  if (!(SRC[0]&RB_ENV_FLAG_PRESET)) {
    return rb_env_decode_long(config,src,srcc);
  }
  
  int attack=(SRC[0]>>5)&3;
  int decay=(SRC[0]>>3)&3;
  int release=SRC[0]&7;
  
  if (!(CONFIG->pointv=malloc(sizeof(struct rb_env_point)*3))) return -1;
  CONFIG->pointa=3;
  CONFIG->pointc=3;
  struct rb_env_point *pt0=CONFIG->pointv+0;
  struct rb_env_point *pt1=CONFIG->pointv+1;
  struct rb_env_point *pt2=CONFIG->pointv+2;
  
  switch (attack) {
    case 0: pt0->time=( 10*config->synth->rate)/1000; pt0->curve=0.0f; break;
    case 1: pt0->time=( 20*config->synth->rate)/1000; pt0->curve=0.0f; break;
    case 2: pt0->time=( 50*config->synth->rate)/1000; pt0->curve=0.0f; break;
    case 3: pt0->time=( 90*config->synth->rate)/1000; pt0->curve=0.0f; break;
  }
  switch (decay) {
    case 0: pt1->time=( 40*config->synth->rate)/1000; pt0->level=0.150f; pt1->level=0.150f; pt1->curve=0.0f; break;
    case 1: pt1->time=( 40*config->synth->rate)/1000; pt0->level=0.250f; pt1->level=0.100f; pt1->curve=0.0f; break;
    case 2: pt1->time=( 40*config->synth->rate)/1000; pt0->level=0.350f; pt1->level=0.090f; pt1->curve=0.0f; break;
    case 3: pt1->time=( 40*config->synth->rate)/1000; pt0->level=0.450f; pt1->level=0.060f; pt1->curve=0.0f; break;
  }
  switch (release) {
    case 0: pt2->time=( 100*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.00f; break;
    case 1: pt2->time=( 150*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.00f; break;
    case 2: pt2->time=( 200*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.00f; break;
    case 3: pt2->time=( 300*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.00f; break;
    case 4: pt2->time=( 450*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.05f; break;
    case 5: pt2->time=( 500*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.10f; break;
    case 6: pt2->time=( 700*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.20f; break;
    case 7: pt2->time=(1200*config->synth->rate)/1000; pt2->level=0.0f; pt2->curve=0.40f; break;
  }
  
  rb_env_digest_points(config);
  
  return 0;
}

/* Fields.
 */
 
static const struct rb_synth_node_field _rb_env_fieldv[]={
  {
    .fldid=RB_ENV_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED|RB_SYNTH_NODE_FIELD_BUF0IFNONE,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_env*)0)->mainv,
  },
  {
    .fldid=RB_ENV_FLDID_mode,
    .name="mode",
    .desc="How do we deal with prior content in main? Default multiply.",
    .enumlabels="mlt,set,add",
    .config_offseti=(uintptr_t)&((struct rb_synth_node_config_env*)0)->mode,
  },
  {
    .fldid=RB_ENV_FLDID_content,
    .name="content",
    .desc="Main body of data, see etc/doc/audio-serial.txt. Short form, may set as integer.",
    .flags=RB_SYNTH_NODE_FIELD_PRINCIPAL,
    .serialfmt=RB_SYNTH_SERIALFMT_ENV,
    .config_sets=_rb_env_set_content,
    .config_offseti=(uintptr_t)&((struct rb_synth_node_config_env*)0)->seriali,
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
