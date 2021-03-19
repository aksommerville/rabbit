#include "rb_aucm_internal.h"
#include "rabbit/rb_serial.h"

/* Generate a blank multiplex program, with its archive wrapper.
 */
 
static int rb_aucm_synthesize_multiplex(
  void *dstpp,
  const struct rb_synth_node_type *type,
  uint8_t programid
) {
  uint8_t *dst=malloc(3);
  if (!dst) return -1;
  dst[0]=programid;
  dst[1]=1;
  dst[2]=type->ntid;
  return 3;
}

/* Measure one range of a multiplex serial field.
 * We fail if it's not properly terminated.
 */
 
static int rb_aucm_measure_multiplex_range(const uint8_t *src,int srcc) {
  // Starts with a 4-byte header, anything goes.
  if (srcc<4) return -1;
  int srcp=4;
  while (1) {
    if (srcp>=srcc) return -1; // Missing terminator.
    if (!src[srcp++]) return srcp; // fldid or terminator.
    struct rb_synth_field_value value;
    int err=rb_synth_field_value_decode(&value,src+srcp,srcc-srcp);
    if (err<1) return -1;
    srcp+=err;
  }
}

/* Combine two encoded multiplex serial fields.
 * Both must consist of one-count ranges only.
 */
 
static int rb_aucm_combine_multiplex_field(
  uint8_t *dst,int dsta,
  const uint8_t *a,int ac,
  const uint8_t *b,int bc
) {
  int dstc=0,ap=0,bp=0;
  while (1) {
  
    // If (a) or (b) is depleted, copy blind from the other and we're done.
    if (ap>ac-4) {
      int cpc=bc-bp;
      if (dstc<=dsta-cpc) memcpy(dst+dstc,b+bp,cpc);
      dstc+=cpc;
      break;
    }
    if (bp>=bc-4) {
      int cpc=ac-ap;
      if (dstc<=dsta-cpc) memcpy(dst+dstc,a+ap,cpc);
      dstc+=cpc;
      break;
    }
    
    // Read the 4-byte header from each input.
    // Counts must be exactly one.
    struct hdr {
      uint8_t srcnoteid;
      uint8_t count;
      uint8_t dstnoteid;
      uint8_t ntid;
    } ahdr,bhdr;
    ahdr.srcnoteid=a[ap+0];
    ahdr.count=    a[ap+1];
    ahdr.dstnoteid=a[ap+2];
    ahdr.ntid=     a[ap+3];
    bhdr.srcnoteid=b[bp+0];
    bhdr.count=    b[bp+1];
    bhdr.dstnoteid=b[bp+2];
    bhdr.ntid=     b[bp+3];
    if ((ahdr.count!=1)||(bhdr.count!=1)) return -1;
    
    // If either input comes before the other, copy it to dst and advance past it.
    if (ahdr.srcnoteid<bhdr.srcnoteid) {
      int err=rb_aucm_measure_multiplex_range(a+ap,ac-ap);
      if (err<4) return -1;
      if (dstc<=dsta-err) memcpy(dst+dstc,a+ap,err);
      dstc+=err;
      ap+=err;
      continue;
    }
    if (bhdr.srcnoteid<ahdr.srcnoteid) {
      int err=rb_aucm_measure_multiplex_range(b+bp,bc-bp);
      if (err<4) return -1;
      if (dstc<=dsta-err) memcpy(dst+dstc,b+bp,err);
      dstc+=err;
      bp+=err;
      continue;
    }
    
    // (b) wins ties, but we must advance both inputs.
    int err=rb_aucm_measure_multiplex_range(a+ap,ac-ap);
    if (err<4) return -1;
    ap+=err;
    if ((err=rb_aucm_measure_multiplex_range(b+bp,bc-bp))<4) return -1;
    if (dstc<=dsta-err) memcpy(dst+dstc,b+bp,err);
    dstc+=err;
    bp+=err;
  }
  if (dstc>dsta) return -1;
  return dstc;
}

/* Combine body of two multiplex programs.
 * Neither is allowed to contain any fields other than the principal.
 * They are allowed to have no fields.
 */
    
static int rb_aucm_combine_multiplex_body(
  uint8_t *dst,int dsta,
  const struct rb_synth_node_type *type,
  const uint8_t *a,int ac,
  const uint8_t *b,int bc
) {

  // Confirm they're the right type, and copy blind if one has no fields.
  if ((ac<1)||(a[0]!=type->ntid)) return -1;
  if ((bc<1)||(b[0]!=type->ntid)) return -1;
  if ((ac==1)||((ac==2)&&!a[1])) {
    if (bc>dsta) return -1;
    memcpy(dst,b,bc);
    return bc;
  } else if ((bc==1)||((bc==2)&&!b[1])) {
    if (ac>dsta) return -1;
    memcpy(dst,a,ac);
    return ac;
  }
  
  // Emit ntid and prepare.
  const struct rb_synth_node_field *field=rb_synth_node_principal_field(type);
  if (!field) return -1;
  if ((dsta<2)||(ac<2)||(bc<2)) return -1;
  dst[0]=type->ntid;
  dst[1]=field->fldid;
  int dstc=2,ap=1,bp=1,err;
  
  // A and B must both be pointing at a definition of this field.
  if (a[ap++]!=field->fldid) return -1;
  if (b[bp++]!=field->fldid) return -1;
  struct rb_synth_field_value avalue,bvalue;
  if ((err=rb_synth_field_value_decode(&avalue,a+ap,ac-ap))<0) return -1; ap+=err;
  if ((err=rb_synth_field_value_decode(&bvalue,b+bp,bc-bp))<0) return -1; bp+=err;
  if (ap<ac) return -1;
  if (bp<bc) return -1;
  if (avalue.logtype!=RB_SYNTH_FIELD_LOG_TYPE_SERIAL) return -1;
  if (bvalue.logtype!=RB_SYNTH_FIELD_LOG_TYPE_SERIAL) return -1;
  
  // OK now the for real combiner.
  int fldlen=rb_aucm_combine_multiplex_field(dst+dstc,dsta-dstc,avalue.v,avalue.c,bvalue.v,bvalue.c);
  if (fldlen<0) return -1;
  if (fldlen<0x100) {
    if (dstc+2+fldlen>dsta) return -1;
    memmove(dst+dstc+2,dst+dstc,fldlen);
    dst[dstc++]=RB_SYNTH_FIELD_TYPE_SERIAL1;
    dst[dstc++]=fldlen;
  } else if (fldlen<0x10000) {
    if (dstc+3+fldlen>dsta) return -1;
    memmove(dst+dstc+3,dst+dstc,fldlen);
    dst[dstc++]=RB_SYNTH_FIELD_TYPE_SERIAL2;
    dst[dstc++]=fldlen>>8;
    dst[dstc++]=fldlen;
  } else {
    return -1;
  }
  dstc+=fldlen;
  
  return dstc;
}

/* Combine two encoded multiplex programs.
 * (a) is an encoded program, no archive header.
 * (b) is an archive with a single program.
 * Either may be null.
 */
 
static int rb_aucm_combine_multiplex(
  void *dstpp,
  const struct rb_synth_node_type *type,
  uint8_t programid,
  const uint8_t *a,int ac,
  const uint8_t *b,int bc
) {
  if (ac<1) {
    if (bc<1) {
      return rb_aucm_synthesize_multiplex(dstpp,type,programid);
    }
    void *dst=malloc(bc);
    if (!dst) return -1;
    memcpy(dst,b,bc);
    *(void**)dstpp=dst;
    return bc;
  } else if (bc<1) {
    int dsta=1+4+ac; // programid,len,body
    uint8_t *dst=malloc(dsta);
    if (!dst) return -1;
    dst[0]=programid;
    int err=rb_vlq_encode(dst+1,dsta-1,ac);
    if (err<1) { free(dst); return -1; }
    int dstc=1+err;
    memcpy(dst+dstc,a,ac);
    dstc+=ac;
    *(void**)dstpp=dst;
    return dstc;
    
  } else {
  
    // Take (a) at face value; it's just the program content.
    // (b) must be an archive with a single program, matching (programid).
    int bp=0,blen=0,err;
    if (bp>=bc) return -1;
    if (b[bp++]!=programid) return -1;
    if ((err=rb_vlq_decode(&blen,b+bp,bc-bp))<1) return -1;
    bp+=err;
    if (bp+blen!=bc) return -1;
    
    // Upper limit of output size is probably (ac+blen+8), but we'll give +16 to be safe.
    int dsta=ac+blen+16;
    uint8_t *dst=malloc(dsta);
    if (!dst) return -1;
    int dstc=0;
    dst[dstc++]=programid;
    
    int bodylen=rb_aucm_combine_multiplex_body(dst+dstc,dsta-dstc,type,a,ac,b+bp,blen);
    if (bodylen<0) {
      free(dst);
      return -1;
    }
    
    // Encode length and combine.
    char vlq[4];
    int vlqc=rb_vlq_encode(vlq,4,bodylen);
    if (vlqc<1) return -1;
    if (dstc+bodylen>dsta-vlqc) return -1;
    memmove(dst+1+vlqc,dst+1,bodylen);
    memcpy(dst+1,vlq,vlqc);
    dstc+=vlqc;
    dstc+=bodylen;
    
    *(void**)dstpp=dst;
    return dstc;
  }
}

/* Compile instrument.
 */
 
int rb_instrument_compile(
  void *dstpp,
  const char *src,int srcc,
  uint8_t programid,
  const char *path
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct rb_aucm aucm={
    .src=src,
    .srcc=srcc,
    .path=path?path:"<unknown>",
    .scope={
      .programid=programid,
    },
  };
  int err=rb_aucm_compile(dstpp,&aucm);
  rb_aucm_cleanup(&aucm);
  return err;
}

/* Compile sound.
 */

int rb_sound_compile(
  void *dstpp,
  const char *src,int srcc,
  uint8_t programid,
  uint8_t noteid,
  const void *program,
  int programc,
  const char *path
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  const struct rb_synth_node_type *type=&rb_synth_node_type_multiplex;
  struct rb_aucm aucm={
    .src=src,
    .srcc=srcc,
    .path=path?path:"<unknown>",
    .scope={
      .programid=programid,
      .type=type,
      .serialfmt=RB_SYNTH_SERIALFMT_NODES,
      .noteid=noteid,
    },
  };
  void *single=0;
  int singlec=rb_aucm_compile(&single,&aucm);
  rb_aucm_cleanup(&aucm);
  if (singlec<0) return singlec;
  int err=rb_aucm_combine_multiplex(dstpp,type,programid,program,programc,single,singlec);
  free(single);
  return err;
}
