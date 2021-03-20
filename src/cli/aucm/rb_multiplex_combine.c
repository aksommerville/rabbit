#include "rabbit/rb_internal.h"
#include "rabbit/rb_serial.h"
#include "rabbit/rb_synth_node.h"
#include "rb_aucm.h"

#define A ((const uint8_t*)a)
#define B ((const uint8_t*)b)

/* Combine two single-program archives.
 */
 
int rb_multiplex_combine_archives(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc) {
  
  // If one is empty, copy the other.
  // Both empty, fail: We can't make one up without the programid.
  if (ac<1) {
    if (bc<1) return -1;
    return rb_encode_raw(dst,b,bc);
  } else if (bc<1) {
    return rb_encode_raw(dst,a,ac);
  }
  
  // (programid) must match. Confirm and emit.
  int aprogramid=A[0];
  int bprogramid=B[0];
  if (aprogramid!=bprogramid) return -1;
  if (rb_encode_u8(dst,aprogramid)<0) return -1;
  int ap=1,bp=1;
  
  // Measure lengths and confirm that that's the entire input.
  int alen,blen,err;
  if ((err=rb_vlq_decode(&alen,A+ap,ac-ap))<1) return -1; ap+=err;
  if (ap+alen!=ac) return -1;
  if ((err=rb_vlq_decode(&blen,B+bp,bc-bp))<1) return -1; bp+=err;
  if (bp+blen!=bc) return -1;
  
  // Combine programs, then insert length.
  if (rb_multiplex_combine_programs(dst,A+ap,alen,B+bp,blen)<0) return -1;
  if (rb_encoder_insert_vlqlen(dst,1)<0) return -1;
  return 0;
}

/* Combine two programs.
 */
 
int rb_multiplex_combine_programs(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc) {

  // If both are empty, make something up. One empty, copy the other blindly.
  if (ac<1) {
    if (bc<1) {
      if (rb_encode_u8(dst,RB_SYNTH_NTID_multiplex)<0) return -1;
      if (rb_encode_u8(dst,0)<0) return -1;
      return 0;
    }
    if (rb_encode_raw(dst,b,bc)<0) return -1;
    return 0;
  }
  if (bc<0) {
    if (rb_encode_raw(dst,a,ac)<0) return -1;
    return 0;
  }
  
  // Get the principal field, don't hard-code its fldid.
  const struct rb_synth_node_type *type=rb_synth_node_type_by_id(RB_SYNTH_NTID_multiplex);
  if (!type) return -1;
  const struct rb_synth_node_field *field=rb_synth_node_principal_field(type);
  if (!field) return -1;
  
  // Validate and emit ntid.
  if (A[0]!=RB_SYNTH_NTID_multiplex) return -1;
  if (B[0]!=RB_SYNTH_NTID_multiplex) return -1;
  if (rb_encode_u8(dst,RB_SYNTH_NTID_multiplex)<0) return -1;
  int ap=1,bp=1,err;
  while (1) {
  
    // If one is EOF or terminated, copy the other blindly.
    if ((ap>=ac)||!A[ap]) {
      if (rb_encode_raw(dst,B+bp,bc-bp)<0) return -1;
      return 0;
    }
    if ((bp>=bc)||!B[bp]) {
      if (rb_encode_raw(dst,A+ap,ac-ap)<0) return -1;
      return 0;
    }
    
    // Both must be pointing to the principal's fldid.
    // We do not allow any other field definitions.
    if (A[ap++]!=field->fldid) return -1;
    if (B[bp++]!=field->fldid) return -1;
  
    // Read the value from each.
    struct rb_synth_field_value avalue,bvalue;
    if ((err=rb_synth_field_value_decode(&avalue,A+ap,ac-ap))<1) return -1; ap+=err;
    if ((err=rb_synth_field_value_decode(&bvalue,B+bp,bc-bp))<1) return -1; bp+=err;
    if (avalue.logtype!=RB_SYNTH_FIELD_LOG_TYPE_SERIAL) return -1;
    if (bvalue.logtype!=RB_SYNTH_FIELD_LOG_TYPE_SERIAL) return -1;
    
    // Prepare the output field header.
    if (rb_encode_u8(dst,field->fldid)<0) return -1;
    int dsthdrp=dst->c;
    
    // Combine ranges.
    if (rb_multiplex_combine_fields(dst,avalue.v,avalue.c,bvalue.v,bvalue.c)<0) return -1;
    
    // Insert field type and length.
    int seriallen=dst->c-dsthdrp;
    if (seriallen<0) return -1;
    if (seriallen>=0x10000) return -1; // whoa
    if (seriallen>=0x100) {
      uint8_t hdr[3]={RB_SYNTH_FIELD_TYPE_SERIAL2,seriallen>>8,seriallen};
      if (rb_encoder_insert(dst,dsthdrp,hdr,sizeof(hdr))<0) return -1;
    } else {
      uint8_t hdr[2]={RB_SYNTH_FIELD_TYPE_SERIAL1,seriallen};
      if (rb_encoder_insert(dst,dsthdrp,hdr,sizeof(hdr))<0) return -1;
    }
    
    // Emit a terminator and finish. If there's other content in (a) or (b), whatever, we don't want it.
    if (rb_encode_u8(dst,0)<0) return -1;
    return 0;
  }
}

/* Measure a multiplex range: (srcnoteid,count,dstnoteid,node)
 */
 
static int rb_multiplex_measure_range(const uint8_t *src,int srcc) {
  if (srcc<4) return -1;
  int srcp=4; // skip ntid, into fields
  while (1) {
    if (srcp>=srcc) return -1; // inner fields must have a terminator
    if (!src[srcp]) return srcp+1; // terminator
    struct rb_synth_field_value value={0};
    int err=rb_synth_field_value_decode(&value,src+srcp,srcc-srcp);
    if (err<1) return -1;
    srcp+=err;
  }
}

/* Combine content of the multiplex principal field.
 * (a,b) are both lists of (srcnoteid,count,dstnoteid,node).
 */
 
int rb_multiplex_combine_fields(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc) {
  int ap=0,bp=0;
  while (1) {
  
    // If one side exhausted, take it on faith that the remainder of the other side is well formed, and copy it.
    if (ap>ac-4) {
      if (rb_encode_raw(dst,B+bp,bc-bp)<0) return -1;
      return 0;
    }
    if (bp>bc-4) {
      if (rb_encode_raw(dst,A+ap,ac-ap)<0) return -1;
      return 0;
    }
    
    // Measure both ranges.
    int alen=rb_multiplex_measure_range(A+ap,ac-ap);
    if (alen<4) return -1;
    int blen=rb_multiplex_measure_range(B+bp,bc-bp);
    if (blen<4) return -1;
    
    // If one (srcnoteid) comes before the other, output it, skip it, and keep the other in place.
    // ...(srcnoteid) is the first byte of the encoded range.
    if (A[ap]<B[bp]) {
      if (rb_encode_raw(dst,A+ap,alen)<0) return -1;
      ap+=alen;
    } else if (A[ap]>B[bp]) {
      if (rb_encode_raw(dst,B+bp,blen)<0) return -1;
      bp+=blen;
    // If they collide, (b) wins, and we skip both.
    } else {
      if (rb_encode_raw(dst,B+bp,blen)<0) return -1;
      ap+=alen;
      bp+=blen;
    }
  }
  return 0;
}

/* Emit an archive by adding an archive over a program.
 * (the aucm use case, where we roll the new content into the live program).
 */
 
int rb_multiplex_add_archive_to_program(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc) {

  // (a) may be empty, and if so (b) is already fully formed.
  if (ac<1) return rb_encode_raw(dst,b,bc);
  
  // Decode (b)'s archive header.
  int bp=0;
  if (bp>=bc) return -1;
  int programid=B[bp++];
  int blen,err;
  if ((err=rb_vlq_decode(&blen,B+bp,bc-bp))<1) return -1;
  bp+=err;
  if (bp+blen!=bc) return -1; // (b) not a single-program archive
  
  // Begin output.
  if (rb_encode_u8(dst,programid)<0) return -1;
  int dstlenp=dst->c;
  
  if (rb_multiplex_combine_programs(dst,a,ac,B+bp,blen)<0) return -1;
  if (rb_encoder_insert_vlqlen(dst,dstlenp)<0) return -1;
  return 0;
}
