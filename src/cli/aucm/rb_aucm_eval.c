#include "rb_aucm_internal.h"

/* Primitive string.
 */
 
int rb_aucm_string_eval(char *dst,int dsta,const char *src,int srcc) {
  if ((srcc<2)||!rb_aucm_isquote(src[0])||(src[0]!=src[srcc-1])) return -1;
  src++;
  srcc-=2;
  int dstc=0,srcp=0;
  while (srcp<srcc) {
    if (src[srcp]=='\\') {
      if (++srcp>=srcc) return -1;
      switch (src[srcp++]) {
        case '\\': case '"': case '\'': case '`': if (dstc<dsta) dst[dstc]=src[srcp-1]; dstc++; break;
        case '0': if (dstc<dsta) dst[dstc]=0x00; dstc++; break;
        case 't': if (dstc<dsta) dst[dstc]=0x09; dstc++; break;
        case 'n': if (dstc<dsta) dst[dstc]=0x0a; dstc++; break;
        case 'r': if (dstc<dsta) dst[dstc]=0x0d; dstc++; break;
        case 'x': {
            if (srcp>srcc-2) return -1;
            int hi=rb_aucm_digit_eval(src[srcp++]);
            int lo=rb_aucm_digit_eval(src[srcp++]);
            if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) return -1;
            if (dstc<dsta) dst[dstc]=(hi<<4)|lo;
            dstc++;
          } break;
        case 'u': {
            int ch=0,digitc=0;
            while (srcp<srcc) {
              int digit=rb_aucm_digit_eval(src[srcp]);
              if ((digit<0)||(digit>15)) break;
              srcp++;
              ch<<=4;
              ch|=digit;
              digitc++;
              if (digitc>6) return -1;
            }
            if (digitc<1) return -1;
            if ((srcp<srcc)&&(src[srcp]==';')) srcp++;
            int err=rb_utf8_encode(dst+dstc,dsta-dstc,ch);
            if (err<0) return -1;
            dstc+=err;
          } break;
        default: return -1;
      }
    } else {
      if (dstc<dsta) dst[dstc]=src[srcp];
      dstc++;
      srcp++;
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Primitive integer.
 */
 
int rb_aucm_int_eval(int *dst,const char *src,int srcc) {
  int positive=1,base=10,srcp=0;
  
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  
  if ((srcp<=srcc-2)&&(src[srcp]=='0')) switch (src[srcp+1]) {
    case 'x': case 'X': base=16; srcp+=2; break;
    case 'd': case 'D': base=10; srcp+=2; break;
    case 'o': case 'O': base=8; srcp+=2; break;
    case 'b': case 'B': base=2; srcp+=2; break;
  }
  
  if (srcp>=srcc) return -1;
  int limit=(positive?(UINT_MAX/base):(INT_MIN/base));
  *dst=0;
  while (srcp<srcc) {
    int digit=rb_aucm_digit_eval(src[srcp]);
    if (digit<0) break;
    if (digit>=base) return -1;
    if (positive) {
      if ((unsigned int)(*dst)>limit) return -1;
      (*dst)*=base;
      if ((unsigned int)(*dst)>UINT_MAX-digit) return -1;
      (*dst)+=digit;
    } else {
      if (*dst<limit) return -1;
      (*dst)*=base;
      if (*dst<INT_MIN+digit) return -1;
      (*dst)-=digit;
    }
    srcp++;
  }
  
  if (srcp>=srcc) return 0;
  return -1;
}

/* Primitive float.
 */
 
int rb_aucm_float_eval(rb_sample_t *dst,const char *src,int srcc) {
  int positive=1,srcp=0;
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    positive=0;
    if (++srcp>=srcc) return -1;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  
  if (!rb_aucm_isdigit(src[srcp])) return -1;
  *dst=0.0f;
  while ((srcp<srcc)&&rb_aucm_isdigit(src[srcp])) {
    (*dst)*=10.0f;
    (*dst)+=src[srcp++]-'0';
  }
  
  if ((srcp<srcc)&&(src[srcp]=='.')) {
    if (++srcp>=srcc) return -1;
    rb_sample_t coef=1.0f;
    while ((srcp<srcc)&&rb_aucm_isdigit(src[srcp])) {
      coef*=0.1f;
      (*dst)+=(src[srcp++]-'0')*coef;
    }
  }
  
  while ((srcp<srcc)&&(rb_aucm_isident(src[srcp])||(src[srcp]=='.'))) srcp++;
  if (srcp<srcc) return -1;
  if (!positive) (*dst)=-*dst;
  return 0;
}

/* Synth node type.
 */
 
const struct rb_synth_node_type *rb_aucm_eval_synth_node_type(struct rb_aucm *aucm) {
  
  if (aucm->token.type==RB_TOKEN_IDENTIFIER) {
    const struct rb_synth_node_type *type=rb_synth_node_type_by_name(aucm->token.v,aucm->token.c);
    if (type) {
      if (rb_aucm_next(aucm)<0) return 0;
      return type;
    }
    
  } else if (aucm->token.type==RB_TOKEN_STRING) {
    char name[256];
    int namec=rb_aucm_string_eval(name,sizeof(name),aucm->token.v,aucm->token.c);
    if (namec<0) {
      rb_aucm_error(aucm,"Failed to evaluate string literal");
      return 0;
    }
    if (namec>sizeof(name)) {
      rb_aucm_error(aucm,"Unreasonably long node type name");
      return 0;
    }
    const struct rb_synth_node_type *type=rb_synth_node_type_by_name(name,namec);
    if (type) {
      if (rb_aucm_next(aucm)<0) return 0;
      return type;
    }
    
  } else if (aucm->token.type==RB_TOKEN_NUMBER) {
    int ntid;
    if (rb_aucm_int_eval(&ntid,aucm->token.v,aucm->token.c)<0) {
      rb_aucm_error(aucm,"Failed to evaluate '%.*s' as integer",aucm->token.c,aucm->token.v);
      return 0;
    }
    if ((ntid<0)||(ntid>0xff)) {
      rb_aucm_error(aucm,"Invalid ntid %d (must be 0..255)",ntid);
      return 0;
    }
    const struct rb_synth_node_type *type=rb_synth_node_type_by_id(ntid);
    if (type) {
      if (rb_aucm_next(aucm)<0) return 0;
      return type;
    }
    
  } else {
    // Punctuation or something
    rb_aucm_error(aucm,"Expected node type name or id");
    return 0;
  }
  // Valid token type but lookup failed
  rb_aucm_error(aucm,"Node type '%.*s' not found",aucm->token.c,aucm->token.v);
  return 0;
}

/* Synth node field.
 */
 
const struct rb_synth_node_field *rb_aucm_eval_synth_node_field(struct rb_aucm *aucm,const struct rb_synth_node_type *type) {
  if (!type) return 0;
  
  if (aucm->token.type==RB_TOKEN_IDENTIFIER) {
    const struct rb_synth_node_field *field=rb_synth_node_field_by_name(type,aucm->token.v,aucm->token.c);
    if (field) {
      if (rb_aucm_next(aucm)<0) return 0;
      return field;
    }
    
  } else if (aucm->token.type==RB_TOKEN_STRING) {
    char name[256];
    int namec=rb_aucm_string_eval(name,sizeof(name),aucm->token.v,aucm->token.c);
    if (namec<0) {
      rb_aucm_error(aucm,"Failed to evaluate string literal");
      return 0;
    }
    if (namec>sizeof(name)) {
      rb_aucm_error(aucm,"Unreasonably long node field name");
      return 0;
    }
    const struct rb_synth_node_field *field=rb_synth_node_field_by_name(type,name,namec);
    if (field) {
      if (rb_aucm_next(aucm)<0) return 0;
      return field;
    }
    
  } else if (aucm->token.type==RB_TOKEN_NUMBER) {
    int fldid;
    if (rb_aucm_int_eval(&fldid,aucm->token.v,aucm->token.c)<0) {
      rb_aucm_error(aucm,"Failed to evaluate '%.*s' as integer",aucm->token.c,aucm->token.v);
      return 0;
    }
    if ((fldid<1)||(fldid>0xff)) {
      rb_aucm_error(aucm,"Invalid fldid %d (must be 1..255)",fldid);
      return 0;
    }
    const struct rb_synth_node_field *field=rb_synth_node_field_by_id(type,fldid);
    if (field) {
      if (rb_aucm_next(aucm)<0) return 0;
      return field;
    }
    
  } else {
    // Punctuation or something
    rb_aucm_error(aucm,"Expected node field name or id");
    return 0;
  }
  // Valid token type but lookup failed
  rb_aucm_error(aucm,"Field '%.*s' not found on type '%s'",aucm->token.c,aucm->token.v,type->name);
  return 0;
}

/* Integer in context.
 */
 
int rb_aucm_eval_int(int *v,struct rb_aucm *aucm,const char *desc,int lo,int hi) {
  if (rb_aucm_int_eval(v,aucm->token.v,aucm->token.c)<0) {
    if (desc) {
      return rb_aucm_error(aucm,"Failed to evaluate '%.*s' as integer for %s",aucm->token.c,aucm->token.v,desc);
    } else {
      return rb_aucm_error(aucm,"Failed to evaluate '%.*s' as integer",aucm->token.c,aucm->token.v);
    }
  }
  if ((*v<lo)||(*v>hi)) {
    char rangedesc[32];
    int rangedescc=0;
    if (lo==INT_MIN) rangedescc=snprintf(rangedesc,sizeof(rangedesc),"max %d",hi);
    else if (hi==INT_MAX) rangedescc=snprintf(rangedesc,sizeof(rangedesc),"min %d",lo);
    else rangedescc=snprintf(rangedesc,sizeof(rangedesc),"%d..%d",lo,hi);
    if (desc) {
      return rb_aucm_error(aucm,"%d out of range for %s (%.*s)",*v,desc,rangedescc,rangedesc);
    } else {
      return rb_aucm_error(aucm,"%d out of range (%.*s)",*v,rangedescc,rangedesc);
    }
  }
  return rb_aucm_next(aucm);
}

/* Float in context.
 */
 
int rb_aucm_eval_float(rb_sample_t *v,struct rb_aucm *aucm,const char *desc,rb_sample_t lo,rb_sample_t hi) {
  if (rb_aucm_float_eval(v,aucm->token.v,aucm->token.c)<0) {
    if (desc) {
      return rb_aucm_error(aucm,"Failed to evaluate '%.*s' as float for %s",aucm->token.c,aucm->token.v,desc);
    } else {
      return rb_aucm_error(aucm,"Failed to evaluate '%.*s' as float",aucm->token.c,aucm->token.v);
    }
  }
  if ((*v<lo)||(*v>hi)) {
    char rangedesc[32];
    int rangedescc=0;
    if (lo==INT_MIN) rangedescc=snprintf(rangedesc,sizeof(rangedesc),"max %f",hi);
    else if (hi==INT_MAX) rangedescc=snprintf(rangedesc,sizeof(rangedesc),"min %f",lo);
    else rangedescc=snprintf(rangedesc,sizeof(rangedesc),"%f..%f",lo,hi);
    if (desc) {
      return rb_aucm_error(aucm,"%f out of range for %s (%.*s)",*v,desc,rangedescc,rangedesc);
    } else {
      return rb_aucm_error(aucm,"%f out of range (%.*s)",*v,rangedescc,rangedesc);
    }
  }
  return rb_aucm_next(aucm);
}
