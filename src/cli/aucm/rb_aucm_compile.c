#include "rb_aucm_internal.h"

/* Compile a block of statements in curly braces.
 * Both braces are expected.
 * Scope is not modified.
 */
 
int rb_aucm_compile_block(struct rb_aucm *aucm) {
  struct rb_token otoken=aucm->token;
  if (aucm->token.type!='{') return rb_aucm_error(aucm,"Expected block");
  if (rb_aucm_next(aucm)<0) return -1;
  while (1) {
    if (!aucm->token.c) {
      aucm->token=otoken;
      return rb_aucm_error(aucm,"Unclosed block");
    }
    if (aucm->token.type=='}') {
      return rb_aucm_next(aucm);
    }
    if (rb_aucm_compile_statement(aucm)<0) return -1;
  }
}

/* Compile value for a field.
 * This emits the encoded type but not the (fldid) introducer.
 */
 
static int rb_aucm_compile_field_value(
  struct rb_aucm *aucm,
  const struct rb_synth_node_type *type,
  const struct rb_synth_node_field *field
) {
  if (rb_aucm_is_help(aucm)) {
    return rb_aucm_help_node_field(aucm,type,field);
  }
  
  // Direct scalar or enum assignment.
  if (aucm->token.type=='=') {
    if (rb_aucm_next(aucm)<0) return -1;
    
    // OK one more chance for "help".
    if (rb_aucm_is_help(aucm)) {
      return rb_aucm_help_node_field(aucm,type,field);
    }
    
    // Try literal integers first, since that is the most restrictive format.
    int iv;
    if (rb_aucm_int_eval(&iv,aucm->token.v,aucm->token.c)>=0) {
      if (field->config_offseti) {
        if ((iv<0)||(iv>0xff)) {
          //TODO We could add a SYNTH_FIELD_TYPE to support larger integers.
          return rb_aucm_error(aucm,
            "Value for %s.%s must be integer in 0..255",type->name,field->name
          );
        }
        if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_U8)<0) return -1;
        if (rb_encode_u8(&aucm->bin,iv)<0) return -1;
        if (rb_aucm_next(aucm)<0) return -1;
        return 0;
      }
      if (field->config_offsetf) {
        if ((iv<-32768)||(iv>32767)) {
          return rb_aucm_error(aucm,
            "Value for %s.%s must be float in -32768..32768",type->name,field->name
          );
        }
        uint8_t serial[5]={RB_SYNTH_FIELD_TYPE_S15_16,iv>>8,iv,0,0};
        if (rb_encode_raw(&aucm->bin,serial,sizeof(serial))<0) return -1;
        if (rb_aucm_next(aucm)<0) return -1;
        return 0;
      }
    }
    
    // Literal float?
    rb_sample_t fv;
    if (rb_aucm_float_eval(&fv,aucm->token.v,aucm->token.c)>=0) {
      if (field->config_offsetf) {
        iv=(int)(fv*65536.0f);
        if (!iv) {
          if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_ZERO)<0) return -1;
        } else if (iv==0x00010000) {
          if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_ONE)<0) return -1;
        } else if (iv==0xffff0000) {
          if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_NONE)<0) return -1;
        } else if (!(iv&0xffff0000)) {
          // TODO We lose 8 bits of precision here, does it matter?
          if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_U0_8)<0) return -1;
          if (rb_encode_u8(&aucm->bin,iv>>8)<0) return -1;
        } else {
          uint8_t serial[5]={RB_SYNTH_FIELD_TYPE_S15_16,iv>>24,iv>>16,iv>>8,iv};
          if (rb_encode_raw(&aucm->bin,serial,sizeof(serial))<0) return -1;
        }
        if (rb_aucm_next(aucm)<0) return -1;
        return 0;
      }
    }
    
    // Check enumlabels.
    if (field->enumlabels&&field->config_offseti) {
      const char *src=field->enumlabels;
      int srcp=0;
      iv=0;
      while (src[srcp]) {
        const char *q=src+srcp;
        int qc=0;
        while (src[srcp]&&(src[srcp]!=',')) { srcp++; qc++; }
        if ((qc==aucm->token.c)&&!memcmp(q,aucm->token.v,qc)) {
          if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_U8)<0) return -1;
          if (rb_encode_u8(&aucm->bin,iv)<0) return -1;
          if (rb_aucm_next(aucm)<0) return -1;
          return 0;
        }
        iv++;
        if (src[srcp]==',') srcp++;
      }
    }
    
    return rb_aucm_error(aucm,
      "Can't find any way to assign '%.*s' to %s.%s as a constant scalar.",
      aucm->token.c,aucm->token.v,type->name,field->name
    );
  }
  
  // Buffer assignment -- rather simpler.
  if (aucm->token.type=='~') {
    if (!field->runner_offsetv) {
      return rb_aucm_error(aucm,
        "Field %s.%s does not support vector assignment.",
        type->name,field->name
      );
    }
    if (rb_aucm_next(aucm)<0) return -1;
    int bufferid;
    if (rb_aucm_eval_int(&bufferid,aucm,"bufferid",0,15)<0) return -1;
    if (rb_encode_u8(&aucm->bin,bufferid)<0) return -1;
    return 0;
  }
  
  // Special tokens "noteid" and "note"
  if ((aucm->token.c==6)&&!memcmp(aucm->token.v,"noteid",6)) {
    if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_NOTEID)<0) return -1;
    if (rb_aucm_next(aucm)<0) return -1;
    return 0;
  }
  if ((aucm->token.c==4)&&!memcmp(aucm->token.v,"note",4)) {
    if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_NOTEHZ)<0) return -1;
    if (rb_aucm_next(aucm)<0) return -1;
    return 0;
  }
  
  return rb_aucm_error(aucm,
    "Expected '=', '~', 'note', or 'noteid' as value for %s.%s",
    type->name,field->name
  );
}

/* Compile node statement.
 */
 
static int rb_aucm_compile_node_statement(struct rb_aucm *aucm) {
  if (rb_aucm_is_help(aucm)) {
    return rb_aucm_help_program_node(aucm);
  }
  struct rb_scope scope0=aucm->scope;
  
  const struct rb_synth_node_type *type=rb_aucm_eval_synth_node_type(aucm);
  if (!type) return -1;
  aucm->scope.type=type;
  aucm->scope.field=0;
  aucm->scope.serialfmt=0;
  aucm->scope.noteid=0;
  
  // Emit ntid.
  if (rb_encode_u8(&aucm->bin,type->ntid)<0) return -1;

  // '=' immediately after TYPE introduces a generic FIELDS block.
  // We've added (type) to (aucm->scope), so that just means a generic block.
  if (aucm->token.type=='=') {
    if (rb_aucm_next(aucm)<0) return -1;
    if (rb_aucm_compile_block(aucm)<0) return -1;
    goto _done_;
  }
  
  // ASSIGNMENTS
  // Each assignment leads with KEY, and the whole set must terminate with semicolon or open curly.
  while (
    (aucm->token.type==RB_TOKEN_IDENTIFIER)||
    (aucm->token.type==RB_TOKEN_NUMBER)||
    (aucm->token.type==RB_TOKEN_STRING)
  ) {
    if (rb_aucm_is_help(aucm)) {
      return rb_aucm_help_node_fields(aucm,type);
    }
    const struct rb_synth_node_field *field=rb_aucm_eval_synth_node_field(aucm,type);
    if (!field) return -1;
    aucm->scope.field=field;
    if (rb_encode_u8(&aucm->bin,field->fldid)<0) return -1;
    if (rb_aucm_compile_field_value(aucm,type,field)<0) return -1;
    if (aucm->token.type==',') {
      if (rb_aucm_next(aucm)<0) return -1;
    }
  }
  
  // Semicolon here terminates the statement.
  if (aucm->token.type==';') {
    if (rb_aucm_next(aucm)<0) return -1;
    goto _done_;
  }
  
  // PRINCIPAL
  if (aucm->token.type!='{') return rb_aucm_error(aucm,
    "Expected ';' or '{' before '%.*s'",aucm->token.c,aucm->token.v
  );
  {
    const struct rb_synth_node_field *field=rb_synth_node_principal_field(type);
    if (!field) return rb_aucm_error(aucm,"Node '%s' has no principal field",type->name);
    aucm->scope.field=field;
    if (!field->serialfmt) {
      aucm->scope.serialfmt=RB_SYNTH_SERIALFMT_HEXDUMP;
    } else {
      aucm->scope.serialfmt=field->serialfmt;
    }
    if (rb_encode_u8(&aucm->bin,field->fldid)<0) return -1;
    if (rb_aucm_compile_statement(aucm)<0) return -1;
    aucm->scope.serialfmt=0;
  }
  
 _done_:;
  if (rb_encode_u8(&aucm->bin,0)<0) return -1; // fields terminator
  aucm->scope=scope0;
  return 0;
}

/* Serial block for one node.
 * TODO I think ntid should be outside the block, can we arrange that?
 * ...the normal cases for this, eg multiplex and instrument, approach it differently.
 * We might not need this type.
 */
 
static int rb_aucm_compile_serial_node(struct rb_aucm *aucm) {
  return rb_aucm_error(aucm,"TODO %s",__func__);//TODO
}

/* Serial block for node list.
 */
 
static int rb_aucm_compile_serial_nodes(struct rb_aucm *aucm) {
  while (1) {
  
    // Terminate on "help", "}", or EOF.
    if (rb_aucm_is_help(aucm)) {
      return rb_aucm_help_inner_node(aucm);
    }
    if (!aucm->token.type||(aucm->token.type=='}')) break;
    
    // Anything else must be a node.
    if (rb_aucm_compile_node_statement(aucm)<0) return -1;
  }
  return 0;
}

/* Serial block for envelope.
 */
 
static int rb_aucm_compile_serial_env(struct rb_aucm *aucm) {

  int format=0; // -1=short, 1=long, 0=undecided
  int attack=0,decay=0,release=0;
  rb_sample_t time=1.0f,level=1.0f;
  int signed_level=0;
  int hires_time=0,hires_level=0,use_curve=0;
  struct rb_env_source_point {
    uint16_t time,level;
    int8_t curve;
  } *pointv=0;
  int pointc=0,pointa=0;
  int err=0;
  
  #define FAIL { err=-1; goto _done_; }

  while (1) {
  
    // Terminate on "help", "}", or EOF.
    if (rb_aucm_is_help(aucm)) {
      if (pointv) free(pointv);
      return rb_aucm_help_env(aucm);
    }
    if (!aucm->token.type||(aucm->token.type=='}')) break;
    
    /* There are seven valid statements, and each belongs to only one format:
     * Short:
     *   attack = 0..3 ;
     *   decay = 0..3 ;
     *   release = 0..7 ;
     * Long:
     *   time = NUMBER ;  // u2.6, time scale, default 1
     *   level = NUMBER ; // u16.8, level scale, default 1
     *   signed_level ;   // If present, LEVEL are signed
     *   TIME LEVEL [CURVE] ;
     */
    #define SCALAR(name,fmt,intmax,fltmax) { \
      if ((aucm->token.c==sizeof(#name)-1)&&!memcmp(aucm->token.v,#name,aucm->token.c)) { \
        if (!format) format=fmt; \
        else if (format!=fmt) { \
          rb_aucm_error(aucm,"'%s' is only legal in %s Format env",#name,(fmt<0)?"Short":"Long"); \
          FAIL \
        } \
        if (rb_aucm_next(aucm)<0) FAIL \
        if (aucm->token.type!='=') { \
          rb_aucm_error(aucm,"Expected '=' after '%s'",#name); \
          FAIL \
        } \
        if (rb_aucm_next(aucm)<0) FAIL \
        if (intmax) { \
          if (rb_aucm_eval_int((void*)&name,aucm,#name,0,intmax)<0) FAIL \
        } else { \
          if (rb_aucm_eval_float((void*)&name,aucm,#name,0.0f,fltmax)<0) FAIL \
        } \
        if (aucm->token.type!=';') { \
          rb_aucm_error(aucm,"';' required to complete statement"); \
          FAIL \
        } \
        if (rb_aucm_next(aucm)<0) FAIL \
        continue; \
      } \
    }
    SCALAR(attack,-1,3,0.0f)
    SCALAR(decay,-1,3,0.0f)
    SCALAR(release,-1,7,0.0f)
    SCALAR(time,1,0,4.0f)
    SCALAR(level,1,0,65536.0f)
    #undef SCALAR
    
    // "signed_level" is like the scalars but no "= VALUE".
    if ((aucm->token.c==12)&&!memcmp(aucm->token.v,"signed_level",12)) {
      if (!format) format=1;
      else if (format!=1) {
        rb_aucm_error(aucm,"'signed_level' is only legal in Long Format env");
        FAIL
      }
      signed_level=1;
      if (rb_aucm_next(aucm)<0) FAIL
      if (aucm->token.type!=';') {
        rb_aucm_error(aucm,"';' required to complete statement");
        FAIL
      }
      if (rb_aucm_next(aucm)<0) FAIL
    }
    
    // Anything else must be a long-format point.
    if (format==-1) {
      rb_aucm_error(aucm,"Unexpected key '%s' in Short Format env",aucm->token.c,aucm->token.v);
      FAIL
    }
    format=1;
    
    // Grab the 2 or 3 tokens we will be using.
    struct rb_token timetoken=aucm->token;
    if (rb_aucm_next(aucm)<0) FAIL
    struct rb_token leveltoken=aucm->token;
    if (rb_aucm_next(aucm)<0) FAIL
    struct rb_token curvetoken={0};
    if (aucm->token.type!=';') {
      curvetoken=aucm->token;
      if (rb_aucm_next(aucm)<0) FAIL
    }
    
    // Evaluate time and level as hexadecimal integers, and curve as a plain integer.
    // Don't log errors yet; just set the values out of range on error.
    int timev=0,levelv=0,curvev=0,i;
    for (i=0;i<timetoken.c;i++) {
      int digit=rb_aucm_digit_eval(timetoken.v[i]);
      if ((digit<0)||(digit>15)) { timev=-1; break; }
      timev<<=4;
      timev|=digit;
    }
    for (i=0;i<leveltoken.c;i++) {
      int digit=rb_aucm_digit_eval(leveltoken.v[i]);
      if ((digit<0)||(digit>15)) { levelv=-1; break; }
      levelv<<=4;
      levelv|=digit;
    }
    if (curvetoken.c) {
      if (rb_aucm_int_eval(&curvev,curvetoken.v,curvetoken.c)<0) curvev=-999;
    }
    
    // If time is invalid, it could be anything. Don't assume they were aiming for time.
    if ((timev<0)||((timetoken.c!=2)&&(timetoken.c!=4))) {
      aucm->token=timetoken;
      rb_aucm_error(aucm,"Unexpected key '%.*s' in Long Format env",timetoken.c,timetoken.v);
      FAIL
    }
    
    if (!timev&&pointc) {
      rb_aucm_error(aucm,"Time zero is only valid for the first point");
      FAIL
    }
    
    // If we have no points yet, this one determines the format.
    if (!pointc) {
      if (timetoken.c==4) hires_time=1;
      if (leveltoken.c==4) hires_level=1;
      if (curvetoken.c) use_curve=1;
    }
    
    // Validate time, level, and curve more aggressively.
    if (
      (timev<0)||
      (hires_time&&(timetoken.c!=4))||
      (!hires_time&&(timetoken.c!=2))
    ) {
      aucm->token=timetoken;
      rb_aucm_error(aucm,"Invalid Long Env time");
      FAIL
    }
    if (
      (levelv<0)||
      (hires_level&&(leveltoken.c!=4))||
      (!hires_level&&(leveltoken.c!=2))
    ) {
      aucm->token=leveltoken;
      rb_aucm_error(aucm,"Invalid Long Env level");
      FAIL
    }
    if (
      (curvev<-128)||(curvev>127)||
      (!use_curve&&curvetoken.c)
    ) {
      aucm->token=curvetoken;
      rb_aucm_error(aucm,"Invalid Long Env curve, must be %s",use_curve?"-128..127":"on all points");
      FAIL
    } else if (use_curve&&!curvetoken.c) {
      rb_aucm_error(aucm,"Curve required for Long Env point");
      FAIL
    }
    
    // Add the point.
    if (pointc>=pointa) {
      pointa+=8;
      if (pointa>INT_MAX/sizeof(struct rb_env_source_point)) FAIL
      void *nv=realloc(pointv,sizeof(struct rb_env_source_point)*pointa);
      if (!nv) FAIL
      pointv=nv;
    }
    struct rb_env_source_point *point=pointv+pointc++;
    point->time=timev;
    point->level=levelv;
    point->curve=curvev;
    
    if (aucm->token.type!=';') {
      rb_aucm_error(aucm,"';' required to complete statement");
      FAIL
    }
    if (rb_aucm_next(aucm)<0) FAIL
  }
  
  // If format is unset -- no statements in the block -- use Short Format with zeroes.
  if (!format) format=-1;
  
  // Emit Short Format.
  if (format<0) {
    uint8_t flags=
      (attack*RB_ENV_PRESET_ATTACK1)|
      (decay*RB_ENV_PRESET_DECAY1)|
      (release*RB_ENV_PRESET_RELEASE1)|
      RB_ENV_FLAG_PRESET;
    if (rb_encode_u8(&aucm->bin,flags)<0) FAIL
    
  // Emit Long Format.
  } else {
    uint8_t flags=0;
    if (time!=1.0f) flags|=RB_ENV_FLAG_TIME_RANGE;
    if (level!=1.0f) flags|=RB_ENV_FLAG_LEVEL_RANGE;
    if ((pointc>=1)&&!pointv[0].time) flags|=RB_ENV_FLAG_INIT_LEVEL;
    if (hires_time) flags|=RB_ENV_FLAG_HIRES_TIME;
    if (hires_level) flags|=RB_ENV_FLAG_HIRES_LEVEL;
    if (signed_level) flags|=RB_ENV_FLAG_SIGNED_LEVEL;
    if (use_curve) flags|=RB_ENV_FLAG_CURVE;
    if (rb_encode_u8(&aucm->bin,flags)<0) FAIL
    
    if (flags&RB_ENV_FLAG_TIME_RANGE) {
      int i=time*64.0f; // u2.6
      if (i>0xff) i=0xff;
      else if (i<0) i=0;
      if (rb_encode_u8(&aucm->bin,i)<0) FAIL
    }
    if (flags&RB_ENV_FLAG_LEVEL_RANGE) {
      int i=level*256.0f; // u16.8
      if (i>0xffffff) i=0xffffff;
      else if (i<0) i=0;
      uint8_t serial[3]={i>>16,i>>8,i};
      if (rb_encode_raw(&aucm->bin,serial,sizeof(serial))<0) FAIL
    }
    int pointp=0;
    if (flags&RB_ENV_FLAG_INIT_LEVEL) {
      if (flags&RB_ENV_FLAG_HIRES_LEVEL) {
        if (rb_encode_u16(&aucm->bin,pointv[0].level)<0) FAIL
      } else {
        if (rb_encode_u8(&aucm->bin,pointv[0].level)<0) FAIL
      }
      pointp=1;
    }
    if (pointp>=pointc) {
      rb_aucm_error(aucm,
        "Long-format env must contain at least one point beyond time zero."
      );
      FAIL
    }
    for (;pointp<pointc;pointp++) {
      const struct rb_env_source_point *point=pointv+pointp;
      if (flags&RB_ENV_FLAG_HIRES_TIME) {
        if (rb_encode_u16(&aucm->bin,point->time)<0) FAIL
      } else {
        if (rb_encode_u8(&aucm->bin,point->time)<0) FAIL
      }
      if (flags&RB_ENV_FLAG_HIRES_LEVEL) {
        if (rb_encode_u16(&aucm->bin,point->level)<0) FAIL
      } else {
        if (rb_encode_u8(&aucm->bin,point->level)<0) FAIL
      }
      if (flags&RB_ENV_FLAG_CURVE) {
        if (rb_encode_u8(&aucm->bin,point->curve)<0) FAIL
      }
    }
  }
  
 _done_:;
  if (pointv) free(pointv);
  return err;
}

/* Serial block for harmonic coefficients.
 */
 
static int rb_aucm_compile_serial_coefv(struct rb_aucm *aucm) {
  while (1) {
  
    // Terminate on "help", "}", or EOF.
    if (rb_aucm_is_help(aucm)) {
      return rb_aucm_help_coefv(aucm);
    }
    if (!aucm->token.type||(aucm->token.type=='}')) break;
    
    // Anything else must be two hex digits.
    int hi=-1,lo=-1;
    if (aucm->token.c==2) {
      hi=rb_aucm_digit_eval(aucm->token.v[0]);
      lo=rb_aucm_digit_eval(aucm->token.v[1]);
    }
    if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) return rb_aucm_error(aucm,
      "Expected a hexadecimal byte or '}'"
    );
    
    if (rb_encode_u8(&aucm->bin,(hi<<4)|lo)<0) return -1;
    if (rb_aucm_next(aucm)<0) return -1;
  }
  return 0;
}

/* Serial block for multiplex ranges.
 */
 
static int rb_aucm_compile_serial_multiplex(struct rb_aucm *aucm) {
  while (1) {
  
    // Terminate on "help", "}", or EOF.
    if (rb_aucm_is_help(aucm)) {
      return rb_aucm_help_multiplex(aucm);
    }
    if (!aucm->token.type||(aucm->token.type=='}')) break;
    
    // Read and emit the 1..3 leading parameters.
    int srcnoteid,srcnoteidmax,dstnoteid;
    if (rb_aucm_eval_int(&srcnoteid,aucm,"srcnoteid",0,127)<0) return -1;
    if (aucm->token.type=='~') {
      if (rb_aucm_next(aucm)<0) return -1;
      if (rb_aucm_eval_int(&srcnoteidmax,aucm,"srcnoteidmax",srcnoteid,127)<0) return -1;
    } else {
      srcnoteidmax=srcnoteid;
    }
    if (aucm->token.type=='>') {
      if (rb_aucm_next(aucm)<0) return -1;
      if (rb_aucm_eval_int(&dstnoteid,aucm,"dstnoteid",0,127)<0) return -1;
    } else {
      dstnoteid=srcnoteid;
    }
    if (rb_encode_u8(&aucm->bin,srcnoteid)<0) return -1;
    if (rb_encode_u8(&aucm->bin,srcnoteidmax-srcnoteid+1)<0) return -1;
    if (rb_encode_u8(&aucm->bin,dstnoteid)<0) return -1;
    
    // ...and the rest is a Node
    if (rb_aucm_compile_node_statement(aucm)<0) return -1;
  }
  return 0;
}

/* Serial block for hex dump.
 */
 
static int rb_aucm_compile_serial_hexdump(struct rb_aucm *aucm) {
  return rb_aucm_error(aucm,"TODO %s",__func__);
}

/* Dispatch on scope.serialfmt and wrap in a generic value.
 */
 
static int rb_aucm_compile_serial_value(struct rb_aucm *aucm) {
  int hdrp=aucm->bin.c;
  
  struct rb_token otoken=aucm->token;
  if (aucm->scope.noteid) {
    // We arrive with a noteid, we're doing the "sound" format. No braces.
  } else {
    if (aucm->token.type!='{') return rb_aucm_error(aucm,
      "Expected '{' to introduce serial block."
    );
    if (rb_aucm_next(aucm)<0) return -1; 
  }
  
  switch (aucm->scope.serialfmt) {
    case 0: return rb_aucm_error(aucm,"%s with serialfmt unset",__func__);
    
    case RB_SYNTH_SERIALFMT_NODE: if (rb_aucm_compile_serial_node(aucm)<0) return -1; break;
    case RB_SYNTH_SERIALFMT_NODES: if (rb_aucm_compile_serial_nodes(aucm)<0) return -1; break;
    case RB_SYNTH_SERIALFMT_ENV: if (rb_aucm_compile_serial_env(aucm)<0) return -1; break;
    case RB_SYNTH_SERIALFMT_COEFV: if (rb_aucm_compile_serial_coefv(aucm)<0) return -1; break;
    case RB_SYNTH_SERIALFMT_MULTIPLEX: if (rb_aucm_compile_serial_multiplex(aucm)<0) return -1; break;
    case RB_SYNTH_SERIALFMT_HEXDUMP: if (rb_aucm_compile_serial_hexdump(aucm)<0) return -1; break;
    
    default: return rb_aucm_error(aucm,
        "Serial format %d not defined [%s:%d]",
        aucm->scope.serialfmt,__FILE__,__LINE__
      );
  }
  
  if (!aucm->scope.noteid) {
    int len=aucm->bin.c-hdrp;
    if (len<0) return rb_aucm_error(aucm,
      "Encoding serialfmt %d reduced output size?",
      aucm->scope.serialfmt
    );
    if (len>0xffff) return rb_aucm_error(aucm,
      "Length %d for serialfmt %d, must not exceed 65535",
      len,aucm->scope.serialfmt
    );
    if ((len==1)&&(aucm->scope.serialfmt==RB_SYNTH_SERIALFMT_ENV)) {
      // Special case for env: 1 byte of output, express it as U8 instead of SERIAL.
      uint8_t hdr[1]={RB_SYNTH_FIELD_TYPE_U8};
      if (rb_encoder_insert(&aucm->bin,hdrp,hdr,sizeof(hdr))<0) return -1;
    } else if (len>0xff) {
      uint8_t hdr[3]={RB_SYNTH_FIELD_TYPE_SERIAL2,len>>8,len};
      if (rb_encoder_insert(&aucm->bin,hdrp,hdr,sizeof(hdr))<0) return -1;
    } else {
      uint8_t hdr[2]={RB_SYNTH_FIELD_TYPE_SERIAL1,len};
      if (rb_encoder_insert(&aucm->bin,hdrp,hdr,sizeof(hdr))<0) return -1;
    }
  
    if (!aucm->token.type) {
      aucm->token=otoken;
      return rb_aucm_error(aucm,"Unclosed serial block.");
    }
    if (aucm->token.type!='}') return rb_aucm_error(aucm,
      "Expected '}' to terminate serial block."
    );
    if (rb_aucm_next(aucm)<0) return -1;
  }
  
  return 0;
}

/* Compile any statement, examines scope.
 */
 
int rb_aucm_compile_statement(struct rb_aucm *aucm) {
  
  // (serialfmt) takes precedence if present.
  if (aucm->scope.serialfmt) {
    return rb_aucm_compile_serial_value(aucm);
  }
  
  // If we have (field), we are compiling everything after KEY.
  if (aucm->scope.field) {
    return rb_aucm_error(aucm,"TODO: Field value");//TODO I don't think this comes up
  }
  
  // If we have (type), expect a field.
  // ASSIGNMENTS and PRINCIPAL must be handled at the start of this Node Statement.
  if (aucm->scope.type) {
    if (rb_aucm_is_help(aucm)) {
      return rb_aucm_help_node_fields(aucm,aucm->scope.type);
    }
    const struct rb_synth_node_field *field=rb_aucm_eval_synth_node_field(aucm,aucm->scope.type);
    if (!field) return -1;
    if (rb_encode_u8(&aucm->bin,field->fldid)<0) return -1;
    aucm->scope.field=field;
    aucm->scope.serialfmt=field->serialfmt;
    if (aucm->token.type=='{') {
      if (rb_aucm_compile_serial_value(aucm)<0) return -1;
    } else {
      if (rb_aucm_compile_field_value(aucm,aucm->scope.type,field)<0) return -1;
      if (aucm->token.type!=';') return rb_aucm_error(aucm,"';' required to complete statement");
      if (rb_aucm_next(aucm)<0) return -1;
    }
    aucm->scope.field=0;
    aucm->scope.serialfmt=0;
    return 0;
  }
  
  // If we have (programid), root scope of instrument, expect a Node Statement.
  // If we also have (noteid), we're at the root scope of a sound, works the same as instrument for us.
  if (aucm->scope.programid<0x80) {
    return rb_aucm_compile_node_statement(aucm);
  }
  
  return rb_aucm_error(aucm,"Scope not initialized");
}

/* Compile in prepared context.
 */
 
int rb_aucm_compile_internal(struct rb_aucm *aucm) {

  // Queue up the first token.
  // If input is empty, output is empty too -- it doesn't require a header or anything.
  // (creating an empty instrument file doesn't actually add that instrument to the archive until you fill it).
  int err=rb_aucm_next(aucm);
  if (err<0) return err;
  if (!err) return 0;
  
  // Emit the archive header.
  if (rb_encode_u8(&aucm->bin,aucm->scope.programid)<0) return -1;
  int pgmlenp=aucm->bin.c;
  
  // If we are a sound, emit the multiplex header.
  int notelenp=-1; // -1 or position of a u16 to overwrite
  if (aucm->scope.noteid) {
    if (rb_encode_u8(&aucm->bin,RB_SYNTH_NTID_multiplex)<0) return -1;
    const struct rb_synth_node_type *mptype=rb_synth_node_type_by_id(RB_SYNTH_NTID_multiplex);
    if (!mptype) return -1;
    const struct rb_synth_node_field *field=rb_synth_node_principal_field(mptype);
    if (!field) return -1;
    if (rb_encode_u8(&aucm->bin,field->fldid)<0) return -1;
    if (rb_encode_u8(&aucm->bin,RB_SYNTH_FIELD_TYPE_SERIAL2)<0) return -1;
    notelenp=aucm->bin.c;
    if (rb_encode_raw(&aucm->bin,"\0\0",2)<0) return -1;
    if (rb_encode_u8(&aucm->bin,aucm->scope.noteid)<0) return -1; // srcnoteid
    if (rb_encode_u8(&aucm->bin,1)<0) return -1; // count
    if (rb_encode_u8(&aucm->bin,aucm->scope.noteid)<0) return -1; // dstnoteid, shouldn't matter
  }
  
  // There can only be one statement at root scope, and it must be a node.
  if ((err=rb_aucm_compile_statement(aucm))<0) return err;
  if (aucm->token.c) return rb_aucm_error(aucm,"Unexpected extra content at root scope.");
  
  // Fill in lengths in the header.
  if (notelenp>=0) {
    int notelen=aucm->bin.c-notelenp-2;
    if ((notelen<0)||(notelen>0xffff)) return -1;
    aucm->bin.v[notelenp+0]=notelen>>8;
    aucm->bin.v[notelenp+1]=notelen;
  }
  if (rb_encoder_insert_vlqlen(&aucm->bin,pgmlenp)<0) return -1;
  
  return 0;
}
