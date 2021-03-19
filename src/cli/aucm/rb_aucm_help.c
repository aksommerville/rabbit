#include "rb_aucm_internal.h"

/* Check for "help" keyword.
 */
 
int rb_aucm_is_help(const struct rb_aucm *aucm) {
  if (aucm->token.c!=4) return 0;
  if (memcmp(aucm->token.v,"help",4)) return 0;
  return 1;
}

/* Begin and end printing help.
 * If begin returns false, don't print anything.
 */
 
static int rb_aucm_help_begin(struct rb_aucm *aucm) {
  if (aucm->help) return 0;
  fprintf(stderr,"+================================================\n");
  fprintf(stderr,"| %s:%d: Aborting due to 'help'\n",aucm->path,aucm->token.lineno);
  fprintf(stderr,"+------------------------------------------------\n");
  return 1;
}

static int rb_aucm_help_end(struct rb_aucm *aucm) {
  if (!aucm->help) {
    fprintf(stderr,"+================================================\n");
    aucm->help=1;
  }
  return RB_CM_FULLY_LOGGED;
}

/* Print description of a node type.
 */
 
static void rb_aucm_describe_node_type(
  struct rb_aucm *aucm,
  const struct rb_synth_node_type *type
) {
  fprintf(stderr,"| 0x%02x %s : %s\n",type->ntid,type->name,type->desc);
}

/* Print description of a node field.
 */
 
static void rb_aucm_describe_node_field(
  struct rb_aucm *aucm,
  const struct rb_synth_node_type *type,
  const struct rb_synth_node_field *field
) {
  fprintf(stderr,"| 0x%02x %s ",field->fldid,field->name);
  if (field->flags&RB_SYNTH_NODE_FIELD_BUF0IFNONE) fprintf(stderr,"OPTIONAL ");
  else if (field->flags&RB_SYNTH_NODE_FIELD_REQUIRED) fprintf(stderr,"REQUIRED ");
  if (field->flags&RB_SYNTH_NODE_FIELD_PRINCIPAL) fprintf(stderr,"PRINCIPAL ");
  fprintf(stderr,": %s\n",field->desc);
}

/* Program node -- root scope.
 */
 
int rb_aucm_help_program_node(struct rb_aucm *aucm) {
  if (rb_aucm_help_begin(aucm)) {
    fprintf(stderr,"| Valid top-level node types:\n");
    int p=0;
    const struct rb_synth_node_type *type;
    for (;type=rb_synth_node_type_by_index(p);p++) {
      if (type->flags&RB_SYNTH_NODE_TYPE_PROGRAM) {
        rb_aucm_describe_node_type(aucm,type);
      }
    }
  }
  return rb_aucm_help_end(aucm);
}

/* Inner node -- inside some container, like instrument or multiplex.
 */
 
int rb_aucm_help_inner_node(struct rb_aucm *aucm) {
  if (rb_aucm_help_begin(aucm)) {
    fprintf(stderr,"| All node types:\n");
    int p=0;
    const struct rb_synth_node_type *type;
    for (;type=rb_synth_node_type_by_index(p);p++) {
      rb_aucm_describe_node_type(aucm,type);
    }
  }
  return rb_aucm_help_end(aucm);
}

/* Fields for a given node type.
 */
 
int rb_aucm_help_node_fields(struct rb_aucm *aucm,const struct rb_synth_node_type *type) {
  if (rb_aucm_help_begin(aucm)) {
    fprintf(stderr,"| Fields for node 0x%02x %s:\n",type->ntid,type->name);
    const struct rb_synth_node_field *field=type->fieldv;
    int i=type->fieldc;
    for (;i-->0;field++) {
      rb_aucm_describe_node_field(aucm,type,field);
    }
  }
  return rb_aucm_help_end(aucm);
}

/* Details of a single node field.
 */
 
static inline const char *rb_aucm_describe_serialfmt(int serialfmt) {
  switch (serialfmt) {
    case RB_SYNTH_SERIALFMT_UNSPEC: return "unspecified";
    case RB_SYNTH_SERIALFMT_NODE: return "node";
    case RB_SYNTH_SERIALFMT_NODES: return "nodes";
    case RB_SYNTH_SERIALFMT_ENV: return "env";
    case RB_SYNTH_SERIALFMT_COEFV: return "coefv";
    case RB_SYNTH_SERIALFMT_MULTIPLEX: return "multiplex";
    case RB_SYNTH_SERIALFMT_HEXDUMP: return "hexdump";
  }
  return "unknown";
}
 
int rb_aucm_help_node_field(
  struct rb_aucm *aucm,
  const struct rb_synth_node_type *type,
  const struct rb_synth_node_field *field
) {
  if (rb_aucm_help_begin(aucm)) {
    fprintf(stderr,"| Field %s.%s (0x%02x.0x%02x):\n",type->name,field->name,type->ntid,field->fldid);
    fprintf(stderr,"| Description: %s\n",field->desc);
    
    fprintf(stderr,"| Flags:");
    if (field->flags&RB_SYNTH_NODE_FIELD_REQUIRED) fprintf(stderr," REQUIRED");
    if (field->flags&RB_SYNTH_NODE_FIELD_BUF0IFNONE) fprintf(stderr," BUF0IFNONE");
    if (field->flags&RB_SYNTH_NODE_FIELD_PRINCIPAL) fprintf(stderr," PRINCIPAL");
    int otherflags=field->flags&~(
      RB_SYNTH_NODE_FIELD_REQUIRED|
      RB_SYNTH_NODE_FIELD_BUF0IFNONE|
      RB_SYNTH_NODE_FIELD_PRINCIPAL|
    0);
    if (otherflags) fprintf(stderr," 0x%08x\n",otherflags);
    else fprintf(stderr,"\n");
    
    if (field->config_sets) {
      fprintf(stderr,"| Serial format %d: %s\n",field->serialfmt,rb_aucm_describe_serialfmt(field->serialfmt));
    }
    
    if (field->config_offseti&&field->enumlabels&&field->enumlabels[0]) {
      fprintf(stderr,"| Enum labels: %s\n",field->enumlabels);
    }
    
    fprintf(stderr,"| Assignment modes:");
    if (field->config_offseti) fprintf(stderr," integer");
    if (field->config_offsetf) fprintf(stderr," scalar");
    if (field->config_sets) fprintf(stderr," serial");
    if (field->runner_offsetv) fprintf(stderr," vector");
    if (field->runner_offsetf) fprintf(stderr," event");
    fprintf(stderr,"\n");
  }
  return rb_aucm_help_end(aucm);
}

/* Env serial.
 */
 
int rb_aucm_help_env(struct rb_aucm *aucm) {
  if (rb_aucm_help_begin(aucm)) {
    fprintf(stderr,"| Env serial format\n");
    fprintf(stderr,"|\n");
    fprintf(stderr,"| Short Form:\n");
    fprintf(stderr,"|   attack = 0..3 ;\n");
    fprintf(stderr,"|   decay = 0..3 ;\n");
    fprintf(stderr,"|   release = 0..7 ;\n");
    fprintf(stderr,"|\n");
    fprintf(stderr,"| Long Form:\n");
    fprintf(stderr,"|   time = 0.0..4.0 ;\n");
    fprintf(stderr,"|   level = 0.0..65536.0 ;\n");
    fprintf(stderr,"|   signed_level ;\n");
    fprintf(stderr,"|   TIME LEVEL [CURVE]\n");
    fprintf(stderr,"|\n");
    fprintf(stderr,"| Long Form points must all be formatted the same way.\n");
    fprintf(stderr,"| TIME and LEVEL are 2 or 4 hexadecimal digits (no prefix).\n");
    fprintf(stderr,"| CURVE is an integer -128..127.\n");
    fprintf(stderr,"| A single point at time zero is permitted, only as the first point.\n");
    fprintf(stderr,"| At least one non-time-zero point is required.\n");
  }
  return rb_aucm_help_end(aucm);
}

/* Harmonic coefficients.
 */
 
int rb_aucm_help_coefv(struct rb_aucm *aucm) {
  if (rb_aucm_help_begin(aucm)) {
    fprintf(stderr,"| Harmonic coefficients.\n");
    fprintf(stderr,"| This is a hex dump where each byte represents a coefficient 0..1.\n");
    fprintf(stderr,"| The fundamental comes first, then the first harmonic, second harmonic, up to 16.\n");
  }
  return rb_aucm_help_end(aucm);
}

/* Multiplex serial.
 */
 
int rb_aucm_help_multiplex(struct rb_aucm *aucm) {
  if (rb_aucm_help_begin(aucm)) {
    fprintf(stderr,"| Multiplex content.\n");
    fprintf(stderr,"| SRCNOTEID [~ SRCNOTEID_MAX] [> DSTNOTEID] NODE\n");
    fprintf(stderr,"| The 1..3 noteid parameters are integers 0..127.\n");
    fprintf(stderr,"| Both optional params default to SRCNOTEID if absent,\n");
    fprintf(stderr,"| ie a single note with the same value inside and out.\n");
  }
  return rb_aucm_help_end(aucm);
}
