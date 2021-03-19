#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"

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
  return rb_synth_error(runner->config->synth,
    "Field %s.%s does not support dynamic scalar assignment.",
    runner->config->type->name,field->name
  );
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

    if (link->type<RB_SYNTH_BUFFER_COUNT) {
      if ((link->type>=bufferc)||!bufferv[link->type]) {
        return rb_synth_error(runner->config->synth,
          "Failed to set %s.%s: Buffer %d was not provided.",
          runner->config->type->name,link->field->name,link->type
        );
      }
      if (!link->field->runner_offsetv) {
        return rb_synth_error(runner->config->synth,
          "Field %s.%s does not support vector assignment.",
          runner->config->type->name,link->field->name
        );
      }
      *(rb_sample_t**)(((char*)runner)+link->field->runner_offsetv)=bufferv[link->type];

    } else switch (link->type) {
      case RB_SYNTH_FIELD_TYPE_NOTEID: if (rb_synth_node_runner_assign(runner,link->field,noteid)<0) return -1; break;
      case RB_SYNTH_FIELD_TYPE_NOTEHZ: if (rb_synth_node_runner_assign(runner,link->field,rb_rate_from_noteid(noteid))<0) return -1; break;
      default: return rb_synth_error(runner->config->synth,
          "Unknown assignment mode 0x%02x for field %s.%s.",
          link->type,runner->config->type->name,link->field->name
        );
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
  if (!config||!config->ready) {
    rb_synth_error(config->synth,"Attempt to instantiate '%s' node, not fully configured.",config->type->name);
    return 0;
  }
  
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
    rb_synth_error(config->synth,"Failed to initialize '%s' runner.",config->type->name);
    rb_synth_node_runner_del(runner);
    return 0;
  }
  if (!runner->update) {
    rb_synth_error(config->synth,"'%s' runner did not get an update hook at init.",config->type->name);
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
  uint8_t type
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
  link->type=type;
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
    return rb_synth_error(config->synth,
      "Field %s.%s does not support constant scalar assignment.",
      config->type->name,field->name
    );
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
    return rb_synth_error(config->synth,
      "Field %s.%s does not support constant integer assignment.",
      config->type->name,field->name
    );
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
    return rb_synth_error(config->synth,
      "Field %s.%s does not support constant serial assignment.",
      config->type->name,field->name
    );
  }
  if (field->config_sets(config,v,c)<0) {
    return rb_synth_error(config->synth,
      "Failed to decode field %s.%s from %d bytes serial.",
      config->type->name,field->name,c
    );
  }
  return 0;
}

/* Decode field.
 */
 
int rb_synth_field_value_decode(struct rb_synth_field_value *dst,const void *src,int srcc) {
  const uint8_t *SRC=src;
  int srcp=0;
  if (srcp>=srcc) return -1;
  memset(dst,0,sizeof(struct rb_synth_field_value));
  dst->type=SRC[srcp++];
  
  // All dynamic assignments are single bytes.
  if (dst->type<0x80) {
    dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_LINK;
    return 1;
  }
  
  switch (dst->type) {
    case RB_SYNTH_FIELD_TYPE_S15_16: {
        if (srcp>srcc-4) return -1;
        int32_t n=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
        srcp+=4;
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_FLOAT;
        dst->f=(rb_sample_t)n/65536.0f;
      } break;
    case RB_SYNTH_FIELD_TYPE_ZERO: {
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_FLOAT;
        dst->f=0.0f;
      } break;
    case RB_SYNTH_FIELD_TYPE_ONE: {
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_FLOAT;
        dst->f=1.0f;
      } break;
    case RB_SYNTH_FIELD_TYPE_NONE: {
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_FLOAT;
        dst->f=-1.0f;
      } break;
    case RB_SYNTH_FIELD_TYPE_SERIAL1: {
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_SERIAL;
        if (srcp>=srcc) return -1;
        dst->c=SRC[srcp++];
        dst->v=SRC+srcp;
        if (srcp>srcc-dst->c) return -1;
        srcp+=dst->c;
      } break;
    case RB_SYNTH_FIELD_TYPE_SERIAL2: {
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_SERIAL;
        if (srcp>srcc-2) return -1;
        dst->c=(SRC[srcp]<<8)|SRC[srcp+1];
        srcp+=2;
        dst->v=SRC+srcp;
        if (srcp>srcc-dst->c) return -1;
        srcp+=dst->c;
      } break;
    case RB_SYNTH_FIELD_TYPE_U8: {
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_INT;
        if (srcp>=srcc) return -1;
        dst->i=SRC[srcp++];
      } break;
    case RB_SYNTH_FIELD_TYPE_U0_8: {
        dst->logtype=RB_SYNTH_FIELD_LOG_TYPE_FLOAT;
        if (srcp>=srcc) return -1;
        dst->f=SRC[srcp++]/255.0f;
      } break;
    default: return -1;
  }
  return srcp;
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
    if (!field) return rb_synth_error(config->synth,
      "Unknown field 0x%02x for node type '%s'.",
      fldid,config->type->name
    );
    
    if ((fldid>=1)&&(fldid<=32)) {
      uint32_t mask=1<<(fldid-1);
      // We could check for duplicate assignment, but actually there are cases where it's ok.
      config->assigned|=mask;
    }
    
    struct rb_synth_field_value value={0};
    if ((err=rb_synth_field_value_decode(&value,SRC+srcp,srcc-srcp))<0) {
      return rb_synth_error(config->synth,
        "Failed to decode value for field %s.%s\n",
        config->type->name,field->name
      );
    }
    srcp+=err;
    
    switch (value.logtype) {
      case RB_SYNTH_FIELD_LOG_TYPE_LINK: if (rb_synth_node_config_add_link(config,field,value.type)<0) return -1; break;
      case RB_SYNTH_FIELD_LOG_TYPE_FLOAT: if (rb_synth_node_config_set_scalar(config,field,value.f)<0) return -1; break;
      case RB_SYNTH_FIELD_LOG_TYPE_INT: if (rb_synth_node_config_set_integer(config,field,value.i)<0) return -1; break;
      case RB_SYNTH_FIELD_LOG_TYPE_SERIAL: if (rb_synth_node_config_set_serial(config,field,value.v,value.c)<0) return -1; break;
      default: return -1;
    }
  }
  return srcp;
}

/* Ready config.
 */

int rb_synth_node_config_ready(struct rb_synth_node_config *config) {
  if (!config) return -1;
  if (config->ready) return 0;
  
  // Check required fields.
  const struct rb_synth_node_field *field=config->type->fieldv;
  int i=config->type->fieldc;
  for (;i-->0;field++) {
    if (field->fldid>32) break;
    if (!(field->flags&RB_SYNTH_NODE_FIELD_REQUIRED)) continue;
    if (config->assigned&(1<<(field->fldid-1))) continue;
    if (field->flags&RB_SYNTH_NODE_FIELD_BUF0IFNONE) {
      if (rb_synth_node_config_add_link(config,field,0x00)<0) return -1;
    } else {
      return rb_synth_error(config->synth,
        "Required field %s.%s was not assigned.",
        config->type->name,field->name
      );
    }
  }
  
  // Let the config do its thing.
  if (config->type->config_ready) {
    if (config->type->config_ready(config)<0) {
      return rb_synth_error(config->synth,
        "Failed to finalize config for '%s' node",config->type->name
      );
    }
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
    return link->type;
  }
  return -1;
}

int rb_synth_node_config_field_is_buffer(const struct rb_synth_node_config *config,uint8_t fldid) {
  const struct rb_synth_node_link *link=config->linkv;
  int i=config->linkc;
  for (;i-->0;link++) {
    if (link->field->fldid!=fldid) continue;
    return (link->type<RB_SYNTH_BUFFER_COUNT);
  }
  return 0;
}
