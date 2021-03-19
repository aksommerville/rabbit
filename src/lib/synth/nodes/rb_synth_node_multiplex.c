#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"

#define RB_MULTIPLEX_FLDID_main 0x01
#define RB_MULTIPLEX_FLDID_ranges 0x02

#define RB_MULTIPLEX_BUFFER_SIZE 1024

/* Instance definition.
 */
 
struct rb_multiplex_range {
  uint8_t srcnoteid;
  uint8_t count;
  uint8_t dstnoteid;
  struct rb_synth_node_config *node;
};
 
struct rb_synth_node_config_multiplex {
  struct rb_synth_node_config hdr;
  struct rb_multiplex_range *rangev;
  int rangec,rangea;
};

struct rb_synth_node_runner_multiplex {
  struct rb_synth_node_runner hdr;
  rb_sample_t *mainv;
  rb_sample_t buf[RB_MULTIPLEX_BUFFER_SIZE];
  struct rb_synth_node_runner *node;
};

#define CONFIG ((struct rb_synth_node_config_multiplex*)config)
#define RUNNER ((struct rb_synth_node_runner_multiplex*)runner)
#define RCONFIG ((struct rb_synth_node_config_multiplex*)(runner->config))

/* Cleanup.
 */
 
static void _rb_multiplex_config_del(struct rb_synth_node_config *config) {
  if (CONFIG->rangev) {
    while (CONFIG->rangec-->0) {
      rb_synth_node_config_del(CONFIG->rangev[CONFIG->rangec].node);
    }
    free(CONFIG->rangev);
  }
}

static void _rb_multiplex_runner_del(struct rb_synth_node_runner *runner) {
  rb_synth_node_runner_del(RUNNER->node);
}

/* Init config.
 */
 
static int _rb_multiplex_config_init(struct rb_synth_node_config *config) {
  return 0;
}

/* Ready config.
 */
 
static int _rb_multiplex_config_ready(struct rb_synth_node_config *config) {
  return 0;
}

/* Update.
 */
 
static void _rb_multiplex_update(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *dst=RUNNER->mainv;
  while (c>RB_MULTIPLEX_BUFFER_SIZE) {
    RUNNER->node->update(RUNNER->node,RB_MULTIPLEX_BUFFER_SIZE);
    memcpy(dst,RUNNER->buf,sizeof(rb_sample_t)*RB_MULTIPLEX_BUFFER_SIZE);
    c-=RB_MULTIPLEX_BUFFER_SIZE;
    dst+=RB_MULTIPLEX_BUFFER_SIZE;
  }
  if (c>0) {
    RUNNER->node->update(RUNNER->node,c);
    memcpy(dst,RUNNER->buf,sizeof(rb_sample_t)*c);
  }
}

/* Find range for note.
 */
 
static int rb_multiplex_rangev_search(
  const struct rb_synth_node_config *config,
  uint8_t noteid
) {
  int lo=0,hi=CONFIG->rangec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rb_multiplex_range *range=CONFIG->rangev+ck;
         if (noteid<range->srcnoteid) hi=ck;
    else if (noteid>=range->srcnoteid+range->count) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}
 
static const struct rb_multiplex_range *rb_multiplex_find_range(
  const struct rb_synth_node_config *config,
  uint8_t noteid
) {
  int p=rb_multiplex_rangev_search(config,noteid);
  if (p<0) return 0;
  return CONFIG->rangev+p;
}

/* Init runner.
 */
 
static int _rb_multiplex_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  
  const struct rb_multiplex_range *range=rb_multiplex_find_range(runner->config,noteid);
  if (!range) return -1;
  uint8_t subnoteid=noteid-range->srcnoteid+range->dstnoteid;
  
  rb_sample_t *buf=RUNNER->buf;
  if (!(RUNNER->node=rb_synth_node_runner_new(
    range->node,
    &buf,1,
    subnoteid
  ))) return -1;
  
  runner->update=_rb_multiplex_update;
  return 0;
}

/* Get duration.
 */
 
static int _rb_multiplex_runner_get_duration(struct rb_synth_node_runner *runner) {
  return rb_synth_node_runner_get_duration(RUNNER->node);
}

/* Set ranges.
 */
 
static int _rb_multiplex_set_ranges(struct rb_synth_node_config *config,const void *src,int srcc) {
  if (CONFIG->rangec) return -1;
  const uint8_t *SRC=src;
  int srcp=0;
  while (srcp<srcc) {
  
    if (srcp>srcc-4) return -1;
    uint8_t srcnoteid=SRC[srcp++];
    uint8_t count=SRC[srcp++];
    uint8_t dstnoteid=SRC[srcp++];
    uint8_t ntid=SRC[srcp++];
    
    const struct rb_synth_node_type *type=rb_synth_node_type_by_id(ntid);
    if (!type) {
      return rb_synth_error(config->synth,
        "Unknown node type 0x%02x requested by multiplex range at note 0x%02x",
        ntid,srcnoteid
      );
    }
    
    if (CONFIG->rangec) {
      const struct rb_multiplex_range *last=CONFIG->rangev+CONFIG->rangec-1;
      if (srcnoteid<last->srcnoteid+last->count) {
        return rb_synth_error(config->synth,
          "Overlapping or missorted ranges in multiplex (%d@0x%02x vs %d@%02x)",
          last->count,last->srcnoteid,count,srcnoteid
        );
      }
    }
    
    if (CONFIG->rangec>=CONFIG->rangea) {
      int na=CONFIG->rangea+8;
      if (na>INT_MAX/sizeof(struct rb_multiplex_range)) return -1;
      void *nv=realloc(CONFIG->rangev,sizeof(struct rb_multiplex_range)*na);
      if (!nv) return -1;
      CONFIG->rangev=nv;
      CONFIG->rangea=na;
    }
    
    struct rb_multiplex_range *range=CONFIG->rangev+CONFIG->rangec++;
    memset(range,0,sizeof(struct rb_multiplex_range));
    
    range->srcnoteid=srcnoteid;
    range->count=count;
    range->dstnoteid=dstnoteid;

    //TODO Is there any value in hanging on to the serial and decoding lazy?
    // I'm picturing a bank of 128 sound effects where only one gets used...
    if (!(range->node=rb_synth_node_config_new(config->synth,type))) return -1;
    int err=rb_synth_node_config_decode_partial(range->node,SRC+srcp,srcc-srcp);
    if (err<0) return -1;
    if (rb_synth_node_config_ready(range->node)<0) return -1;
    srcp+=err;
  }
  return 0;
}

/* Fields.
 */
 
static const struct rb_synth_node_field _rb_multiplex_fieldv[]={
  {
    .fldid=RB_MULTIPLEX_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED|RB_SYNTH_NODE_FIELD_BUF0IFNONE,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_multiplex*)0)->mainv,
  },
  {
    .fldid=RB_MULTIPLEX_FLDID_ranges,
    .name="ranges",
    .desc="Distinct notes or ranges of notes sharing one configuration",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED|RB_SYNTH_NODE_FIELD_PRINCIPAL,
    .serialfmt=RB_SYNTH_SERIALFMT_MULTIPLEX,
    .config_sets=_rb_multiplex_set_ranges,
  },
};

/* Type definition.
 */
 
const struct rb_synth_node_type rb_synth_node_type_multiplex={
  .ntid=RB_SYNTH_NTID_multiplex,
  .name="multiplex",
  .desc="Combine independent programs, eg for drum kits or sound effects.",
  .flags=RB_SYNTH_NODE_TYPE_PROGRAM,
  .config_objlen=sizeof(struct rb_synth_node_config_multiplex),
  .runner_objlen=sizeof(struct rb_synth_node_runner_multiplex),
  .fieldv=_rb_multiplex_fieldv,
  .fieldc=sizeof(_rb_multiplex_fieldv)/sizeof(struct rb_synth_node_field),
  .config_del=_rb_multiplex_config_del,
  .runner_del=_rb_multiplex_runner_del,
  .config_init=_rb_multiplex_config_init,
  .config_ready=_rb_multiplex_config_ready,
  .runner_init=_rb_multiplex_runner_init,
  .runner_get_duration=_rb_multiplex_runner_get_duration,
};
