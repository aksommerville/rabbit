#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"

//TODO Would be polite of us to log errors or something.

/* Trivial lifecycle.
 */

void rb_synth_node_runner_del(struct rb_synth_node_runner *runner) {
  if (!runner) return;
  if (runner->refc-->1) return;
  if (runner->config->type->runner_del) runner->config->type->runner_del(runner);
  free(runner);
}

int rb_synth_node_runner_ref(struct rb_synth_node_runner *runner) {
  if (!runner) return -1;
  if (runner->refc<1) return -1;
  if (runner->refc==INT_MAX) return -1;
  runner->refc++;
  return 0;
}

void rb_synth_node_config_del(struct rb_synth_node_config *config) {
  if (!config) return;
  if (config->refc-->1) return;
  if (config->type->config_del) config->type->config_del(config);
  if (config->linkv) free(config->linkv);
  free(config);
}

int rb_synth_node_config_ref(struct rb_synth_node_config *config) {
  if (!config) return -1;
  if (config->refc<1) return -1;
  if (config->refc==INT_MAX) return -1;
  config->refc++;
  return 0;
}

/* Assign scalar runner field.
 */
 
static int rb_synth_node_runner_assign(
  struct rb_synth_node_runner *runner,
  const struct rb_synth_node_field *field,
  rb_sample_t v
) {
  if (field->runner_offsetf) {
    *(rb_sample_t*)(((char*)runner)+field->runner_offsetf)=v;
    return 0;
  }
  // field does not support dynamic scalar assignment
  return -1;
}

/* Link runner, pre-init.
 */
 
static int rb_synth_node_runner_link(
  struct rb_synth_node_runner *runner,
  rb_sample_t **bufferv,int bufferc,
  uint8_t noteid
) {
  const struct rb_synth_node_link *link=runner->config->linkv;
  int i=runner->config->linkc;
  for (;i-->0;link++) {

    if (link->bufferid<RB_SYNTH_BUFFER_COUNT) {
      if ((link->bufferid>=bufferc)||!bufferv[link->bufferid]) {
        // requested buffer not provided by parent
        return -1;
      }
      if (!link->field->runner_offsetv) {
        // field does not support buffer assignment
        return -1;
      }
      *(rb_sample_t**)(((char*)runner)+link->field->runner_offsetv)=bufferv[link->bufferid];

    } else switch (link->bufferid) {
      case RB_SYNTH_LINK_NOTEID: if (rb_synth_node_runner_assign(runner,link->field,noteid)<0) return -1; break;
      case RB_SYNTH_LINK_NOTEHZ: if (rb_synth_node_runner_assign(runner,link->field,rb_rate_from_noteid(noteid))<0) return -1; break;
      default: return -1;
    }
  }
  return 0;
}

/* New runner.
 */

struct rb_synth_node_runner *rb_synth_node_runner_new(
  struct rb_synth_node_config *config,
  rb_sample_t **bufferv,int bufferc,
  uint8_t noteid
) {
  if (!config||!config->ready) return 0;
  
  struct rb_synth_node_runner *runner=calloc(1,config->type->runner_objlen);
  if (!runner) return 0;
  
  if (rb_synth_node_config_ref(config)<0) {
    free(runner);
    return 0;
  }
  runner->config=config;
  runner->refc=1;
  
  if (rb_synth_node_runner_link(runner,bufferv,bufferc,noteid)<0) {
    rb_synth_node_runner_del(runner);
    return 0;
  }
  
  if (config->type->runner_init(runner,noteid)<0) {
    rb_synth_node_runner_del(runner);
    return 0;
  }
  if (!runner->update) {
    rb_synth_node_runner_del(runner);
    return 0;
  }
  
  return runner;
}

/* Runner hooks.
 */
 
int rb_synth_node_runner_get_duration(struct rb_synth_node_runner *runner) {
  if (!runner) return -1;
  if (!runner->config->type->runner_get_duration) return -1;
  return runner->config->type->runner_get_duration(runner);
}

/* New config.
 */

struct rb_synth_node_config *rb_synth_node_config_new(
  struct rb_synth *synth,
  const struct rb_synth_node_type *type
) {
  if (!type) return 0;
  struct rb_synth_node_config *config=calloc(1,type->config_objlen);
  if (!config) return 0;
  
  config->synth=synth;
  config->type=type;
  config->refc=1;
  
  if (type->config_init) {
    if (type->config_init(config)<0) {
      rb_synth_node_config_del(config);
      return 0;
    }
  }
  
  return config;
}

/* Convenience: New, decode, ready.
 */
 
struct rb_synth_node_config *rb_synth_node_config_new_decode(
  struct rb_synth *synth,
  const void *src,int srcc
) {
  if (!synth||!src||(srcc<1)) return 0;
  const uint8_t *SRC=src;
  const struct rb_synth_node_type *type=rb_synth_node_type_by_id(SRC[0]);
  if (!type) {
    // unknown node type
    return 0;
  }
  struct rb_synth_node_config *config=rb_synth_node_config_new(synth,type);
  if (!config) return 0;
  if (
    (rb_synth_node_config_decode_partial(config,SRC+1,srcc-1)<0)||
    (rb_synth_node_config_ready(config)<0)
  ) {
    rb_synth_node_config_del(config);
    return 0;
  }
  return config;
}

/* Add link.
 */
 
static int rb_synth_node_config_add_link(
  struct rb_synth_node_config *config,
  const struct rb_synth_node_field *field,
  uint8_t bufferid
) {
  //TODO We could check assignment supported here.
  // It will get checked at instantiation too, but this might be a more helpful spot for logging.
  if (config->linkc>=config->linka) {
    int na=config->linka+4;
    if (na>INT_MAX/sizeof(struct rb_synth_node_link)) return -1;
    void *nv=realloc(config->linkv,sizeof(struct rb_synth_node_link)*na);
    if (!nv) return -1;
    config->linkv=nv;
    config->linka=na;
  }
  struct rb_synth_node_link *link=config->linkv+config->linkc++;
  link->field=field;
  link->bufferid=bufferid;
  return 0;
}

/* Set scalar config field.
 */
 
static int rb_synth_node_config_set_scalar(
  struct rb_synth_node_config *config,
  const struct rb_synth_node_field *field,
  rb_sample_t v
) {
  if (!field->config_offsetf) {
    // field does not support constant scalar assignment
    return -1;
  }
  *(rb_sample_t*)(((char*)config)+field->config_offsetf)=v;
  return 0;
}

/* Set integer config field.
 */
 
static int rb_synth_node_config_set_integer(
  struct rb_synth_node_config *config,
  const struct rb_synth_node_field *field,
  int v
) {
  if (!field->config_offseti) {
    // field does not support constant integer assignment
    return -1;
  }
  *(int*)(((char*)config)+field->config_offseti)=v;
  return 0;
}

/* Set serial config field.
 */
 
static int rb_synth_node_config_set_serial(
  struct rb_synth_node_config *config,
  const struct rb_synth_node_field *field,
  const void *v,int c
) {
  if (!field->config_sets) {
    // field does not support constant serial assignment
    return -1;
  }
  if (field->config_sets(config,v,c)<0) {
    // failed to decode field
    return -1;
  }
  return 0;
}

/* Decode config.
 */
 
int rb_synth_node_config_decode_partial(
  struct rb_synth_node_config *config,
  const void *src,int srcc
) {
  if (!config||config->ready) return -1;
  const uint8_t *SRC=src;
  int srcp=0,err;
  while (srcp<srcc) {
  
    uint8_t fldid=SRC[srcp++];
    if (!fldid) break;
    const struct rb_synth_node_field *field=rb_synth_node_field_by_id(config->type,fldid);
    if (!field) {
      // unknown field
      return -1;
    }
    
    if (srcp>=srcc) {
      // expected field format
      return -1;
    }
    uint8_t lead=SRC[srcp++];
    if (lead<RB_SYNTH_BUFFER_COUNT) {
      if (rb_synth_node_config_add_link(config,field,lead)<0) return -1;
    } else switch (lead) {
      case RB_SYNTH_LINK_NOTEID: if (rb_synth_node_config_add_link(config,field,lead)<0) return -1; break;
      case RB_SYNTH_LINK_NOTEHZ: if (rb_synth_node_config_add_link(config,field,lead)<0) return -1; break;
      
      case RB_SYNTH_LINK_S15_16: {
          if (srcp>srcc-4) {
            // failed to decode scalar constant
            return -1;
          }
          int32_t iv=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
          srcp+=4;
          rb_sample_t v=iv/65536.0f;
          if (rb_synth_node_config_set_scalar(config,field,v)<0) return -1;
        } break;
        
      case RB_SYNTH_LINK_ZERO: if (rb_synth_node_config_set_scalar(config,field,0.0f)<0) return -1; break;
      case RB_SYNTH_LINK_ONE: if (rb_synth_node_config_set_scalar(config,field,1.0f)<0) return -1; break;
      case RB_SYNTH_LINK_NONE: if (rb_synth_node_config_set_scalar(config,field,-1.0f)<0) return -1; break;
      
      case RB_SYNTH_LINK_SERIAL1: {
          if (srcp>srcc-1) {
            // failed to decode serial constant
            return -1;
          }
          int len=SRC[srcp++];
          if (srcp>srcc-len) {
            // failed to decode serial constant
            return -1;
          }
          if (rb_synth_node_config_set_serial(config,field,SRC+srcp,len)<0) return -1;
          srcp+=len;
        } break;
        
      case RB_SYNTH_LINK_SERIAL2: {
          if (srcp>srcc-2) {
            // failed to decode serial constant
            return -1;
          }
          int len=(SRC[srcp]<<8)|SRC[srcp+1];
          srcp+=2;
          if (srcp>srcc-len) {
            // failed to decode serial constant
            return -1;
          }
          if (rb_synth_node_config_set_serial(config,field,SRC+srcp,len)<0) return -1;
          srcp+=len;
        } break;
        
      case RB_SYNTH_LINK_U8: {
          if (srcp>srcc-1) {
            // failed to decode integer
            return -1;
          }
          int v=SRC[srcp++];
          if (rb_synth_node_config_set_integer(config,field,v)<0) return -1;
        } break;
        
      default: {
          // unknown field format
          return -1;
        }
    }
  }
  return srcp;
}

/* Ready config.
 */

int rb_synth_node_config_ready(struct rb_synth_node_config *config) {
  if (!config) return -1;
  if (config->ready) return 0;
  if (config->type->config_ready) {
    if (config->type->config_ready(config)<0) return -1;
  }
  config->ready=1;
  return 0;
}

/* Examine config links.
 */

int rb_synth_node_config_find_link(const struct rb_synth_node_config *config,uint8_t fldid) {
  const struct rb_synth_node_link *link=config->linkv;
  int i=config->linkc;
  for (;i-->0;link++) {
    if (link->field->fldid!=fldid) continue;
    return link->bufferid;
  }
  return -1;
}

int rb_synth_node_config_field_is_buffer(const struct rb_synth_node_config *config,uint8_t fldid) {
  const struct rb_synth_node_link *link=config->linkv;
  int i=config->linkc;
  for (;i-->0;link++) {
    if (link->field->fldid!=fldid) continue;
    return (link->bufferid<RB_SYNTH_BUFFER_COUNT);
  }
  return 0;
}
