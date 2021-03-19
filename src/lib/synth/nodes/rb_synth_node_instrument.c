#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"

#define RB_INSTRUMENT_FLDID_main 0x01
#define RB_INSTRUMENT_FLDID_nodes 0x02

#define RB_INSTRUMENT_BUFFER_SIZE 1024

/* Instance definition.
 */
 
struct rb_synth_node_config_instrument {
  struct rb_synth_node_config hdr;
  uint16_t bufmask; // bitmask of required buffers, 1<<bufferid
  struct rb_synth_node_config **childv;
  int childc,childa;
};

struct rb_synth_node_runner_instrument {
  struct rb_synth_node_runner hdr;
  uint8_t noteid;
  rb_sample_t *mainv;
  rb_sample_t *bufv[RB_SYNTH_BUFFER_COUNT]; // sparse, see config->bufmask.
  struct rb_synth_node_runner **childv; // length in config
};

#define CONFIG ((struct rb_synth_node_config_instrument*)config)
#define RUNNER ((struct rb_synth_node_runner_instrument*)runner)
#define RCONFIG ((struct rb_synth_node_config_instrument*)(runner->config))

/* Cleanup.
 */
 
static void _rb_instrument_config_del(struct rb_synth_node_config *config) {
  if (CONFIG->childv) {
    while (CONFIG->childc-->0) {
      rb_synth_node_config_del(CONFIG->childv[CONFIG->childc]);
    }
    free(CONFIG->childv);
  }
}

static void _rb_instrument_runner_del(struct rb_synth_node_runner *runner) {
  {
    int i=RB_SYNTH_BUFFER_COUNT;
    while (i-->0) {
      if (RUNNER->bufv[i]) free(RUNNER->bufv[i]);
    }
  }
  if (RUNNER->childv) {
    int i=RCONFIG->childc;
    while (i-->0) {
      rb_synth_node_runner_del(RUNNER->childv[i]);
    }
    free(RUNNER->childv);
  }
}

/* Init config.
 */
 
static int _rb_instrument_config_init(struct rb_synth_node_config *config) {

  CONFIG->bufmask=0x0001; // bufv[0] has to exist whether someone asks for it or not

  return 0;
}

/* Initialize (bufmask) based on children's declarations.
 */
 
static void rb_instrument_set_bufmask_1(
  struct rb_synth_node_config *config,
  const struct rb_synth_node_config *child
) {
  const struct rb_synth_node_link *link=child->linkv;
  int i=child->linkc;
  for (;i-->0;link++) {
    if (link->type<RB_SYNTH_BUFFER_COUNT) {
      CONFIG->bufmask|=1<<link->type;
    }
  }
}
 
static void rb_instrument_set_bufmask(struct rb_synth_node_config *config) {
  CONFIG->bufmask=0x0001;
  struct rb_synth_node_config **child=CONFIG->childv;
  int i=CONFIG->childc;
  for (;i-->0;child++) {
    rb_instrument_set_bufmask_1(config,*child);
  }
}

/* Ready config.
 */
 
static int _rb_instrument_config_ready(struct rb_synth_node_config *config) {
  rb_instrument_set_bufmask(config);
  return 0;
}

/* Spawn child config.
 */
 
static struct rb_synth_node_config *rb_instrument_config_spawn(
  struct rb_synth_node_config *config,
  const struct rb_synth_node_type *type
) {
  if (CONFIG->childc>=CONFIG->childa) {
    int na=CONFIG->childa+8;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(CONFIG->childv,sizeof(void*)*na);
    if (!nv) return 0;
    CONFIG->childv=nv;
    CONFIG->childa=na;
  }
  struct rb_synth_node_config *child=rb_synth_node_config_new(config->synth,type);
  if (!child) return 0;
  CONFIG->childv[CONFIG->childc++]=child;
  return child;
}

/* Set nodes.
 */
 
static int _rb_instrument_set_nodes(struct rb_synth_node_config *config,const void *src,int srcc) {
  const uint8_t *SRC=src;
  int srcp=0;
  while (srcp<srcc) {
    uint8_t ntid=SRC[srcp++];
    const struct rb_synth_node_type *type=rb_synth_node_type_by_id(ntid);
    if (!type) return rb_synth_error(config->synth,
      "Unknown synth node type 0x%02x",ntid
    );
    struct rb_synth_node_config *child=rb_instrument_config_spawn(config,type);
    if (!child) return -1;
    int err=rb_synth_node_config_decode_partial(child,SRC+srcp,srcc-srcp);
    if (err<0) return -1;
    srcp+=err;
    if (rb_synth_node_config_ready(child)<0) return -1;
  }
  return 0;
}

/* Update.
 */
 
static void rb_instrument_update_children(rb_sample_t *v,int c,struct rb_synth_node_runner *runner) {
  struct rb_synth_node_runner **child=RUNNER->childv;
  int i=RCONFIG->childc;
  for (;i-->0;child++) {
    (*child)->update(*child,c);
  }
  memcpy(v,RUNNER->bufv[0],sizeof(rb_sample_t)*c);
}
 
static void _rb_instrument_update(struct rb_synth_node_runner *runner,int c) {
  rb_sample_t *dst=RUNNER->mainv;
  while (c>RB_INSTRUMENT_BUFFER_SIZE) {
    rb_instrument_update_children(dst,RB_INSTRUMENT_BUFFER_SIZE,runner);
    c-=RB_INSTRUMENT_BUFFER_SIZE;
    dst+=RB_INSTRUMENT_BUFFER_SIZE;
  }
  if (c>0) {
    rb_instrument_update_children(dst,c,runner);
  }
}

/* Allocate buffers.
 */
 
static int rb_instrument_allocate_buffers(struct rb_synth_node_runner *runner) {
  int i=0;
  uint16_t mask=0x0001;
  rb_sample_t **v=RUNNER->bufv;
  for (;i<RB_SYNTH_BUFFER_COUNT;i++,mask<<=1,v++) {
    if (RCONFIG->bufmask&mask) {
      if (!(*v=malloc(sizeof(rb_sample_t)*RB_INSTRUMENT_BUFFER_SIZE))) return -1;
    }
  }
  // Clear [0] just in case our user fucked up and forgot to write anything there.
  memset(RUNNER->bufv[0],0,sizeof(rb_sample_t)*RB_INSTRUMENT_BUFFER_SIZE);
  return 0;
}

/* Allocate runner->childv and instantiate contents from config.
 */
 
static int rb_instrument_instantiate_children(struct rb_synth_node_runner *runner) {
  if (RCONFIG->childc<1) return 0;
  if (!(RUNNER->childv=calloc(sizeof(void*),RCONFIG->childc))) return -1;
  int i=0; for (;i<RCONFIG->childc;i++) {
    if (!(RUNNER->childv[i]=rb_synth_node_runner_new(
      RCONFIG->childv[i],
      RUNNER->bufv,RB_SYNTH_BUFFER_COUNT,
      RUNNER->noteid
    ))) return -1;
  }
  return 0;
}

/* Init runner.
 */
 
static int _rb_instrument_runner_init(struct rb_synth_node_runner *runner,uint8_t noteid) {
  if (!RUNNER->mainv) return -1;
  
  RUNNER->noteid=noteid;
  
  if (rb_instrument_allocate_buffers(runner)<0) return -1;
  if (rb_instrument_instantiate_children(runner)<0) return -1;
  
  runner->update=_rb_instrument_update;
  return 0;
}

/* Get duration.
 * The last child to report a duration wins.
 * So a typical setup like [oscillator,envelope,filter] should pick up the envelope.
 */
 
static int _rb_instrument_runner_get_duration(struct rb_synth_node_runner *runner) {
  int i=RCONFIG->childc;
  while (i-->0) {
    struct rb_synth_node_runner *child=RUNNER->childv[i];
    int duration=rb_synth_node_runner_get_duration(child);
    if (duration>0) return duration;
  }
  return -1;
}

/* Fields.
 */
 
static const struct rb_synth_node_field _rb_instrument_fieldv[]={
  {
    .fldid=RB_INSTRUMENT_FLDID_main,
    .name="main",
    .desc="Output.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED|RB_SYNTH_NODE_FIELD_BUF0IFNONE,
    .runner_offsetv=(uintptr_t)&((struct rb_synth_node_runner_instrument*)0)->mainv,
  },
  {
    .fldid=RB_INSTRUMENT_FLDID_nodes,
    .name="nodes",
    .desc="Encoded child nodes, in order of operation.",
    .flags=RB_SYNTH_NODE_FIELD_REQUIRED|RB_SYNTH_NODE_FIELD_PRINCIPAL,
    .serialfmt=RB_SYNTH_SERIALFMT_NODES,
    .config_sets=_rb_instrument_set_nodes,
  },
};

/* Type definition.
 */
 
const struct rb_synth_node_type rb_synth_node_type_instrument={
  .ntid=RB_SYNTH_NTID_instrument,
  .name="instrument",
  .desc="Generic tuned instrument, each note plays the same program at a different pitch.",
  .flags=RB_SYNTH_NODE_TYPE_PROGRAM,
  .config_objlen=sizeof(struct rb_synth_node_config_instrument),
  .runner_objlen=sizeof(struct rb_synth_node_runner_instrument),
  .fieldv=_rb_instrument_fieldv,
  .fieldc=sizeof(_rb_instrument_fieldv)/sizeof(struct rb_synth_node_field),
  .config_del=_rb_instrument_config_del,
  .runner_del=_rb_instrument_runner_del,
  .config_init=_rb_instrument_config_init,
  .config_ready=_rb_instrument_config_ready,
  .runner_init=_rb_instrument_runner_init,
  .runner_get_duration=_rb_instrument_runner_get_duration,
};
