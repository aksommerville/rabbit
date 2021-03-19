#include "rabbit/rb_internal.h"
#include "rabbit/rb_serial.h"

/* Decode MIDI-compatible VLQ.
 */
 
int rb_vlq_decode(int *dst,const void *src,int srcc) {
  if (srcc<1) return 0;
  const uint8_t *SRC=src;
  
  if (!(SRC[0]&0x80)) {
    *dst=SRC[0];
    return 1;
  }
  
  if (srcc<2) return 0;
  if (!(SRC[1]&0x80)) {
    *dst=((SRC[0]&0x7f)<<7)|SRC[1];
    return 2;
  }
  
  if (srcc<3) return 0;
  if (!(SRC[2]&0x80)) {
    *dst=((SRC[0]&0x7f)<<14)|((SRC[1]&0x7f)<<7)|SRC[2];
    return 3;
  }
  
  if (srcc<4) return 0;
  if (!(SRC[3]&0x80)) {
    *dst=((SRC[0]&0x7f)<<21)|((SRC[1]&0x7f)<<14)|((SRC[2]&0x7f)<<7)|SRC[3];
    return 4;
  }
  return -1;
}

/* Encode VLQ.
 */
 
int rb_vlq_encode(void *dst,int dsta,int src) {
  if (!dst||(dsta<0)) dsta=0;
  uint8_t *DST=dst;
  if (src<0) return -1;
  if (src<0x80) {
    if (dsta>=1) {
      DST[0]=src;
    }
    return 1;
  }
  if (src<0x4000) {
    if (dsta>=2) {
      DST[0]=0x80|(src>>7);
      DST[1]=src&0x7f;
    }
    return 2;
  }
  if (src<0x200000) {
    if (dsta>=3) {
      DST[0]=0x80|(src>>14);
      DST[1]=0x80|((src>>7)&0x7f);
      DST[2]=src&0x7f;
    }
    return 3;
  }
  if (src<0x10000000) {
    if (dsta>=4) {
      DST[0]=0x80|(src>>21);
      DST[1]=0x80|((src>>14)&0x7f);
      DST[2]=0x80|((src>>7)&0x7f);
      DST[3]=src&0x7f;
    }
    return 4;
  }
  return -1;
}

/* Encode UTF-8.
 */
 
int rb_utf8_encode(void *dst,int dsta,int ch) {
  if (!dst||(dsta<0)) dsta=0;
  uint8_t *DST=dst;
  if (ch<0) return -1;
  if (ch<0x80) {
    if (dsta>=1) {
      DST[0]=ch;
    }
    return 1;
  }
  if (ch<0x800) {
    if (dsta>=2) {
      DST[0]=0xc0|(ch>>6);
      DST[1]=ch&0x3f;
    }
    return 2;
  }
  if (ch<0x1000) {
    if (dsta>=3) {
      DST[0]=0xe0|(ch>>12);
      DST[1]=0x80|((ch>>6)&0x3f);
      DST[2]=ch&0x3f;
    }
    return 3;
  }
  if (ch<0x110000) {
    if (dsta>=4) {
      DST[0]=0xf0|(ch>>18);
      DST[1]=0x80|((ch>>12)&0x3f);
      DST[2]=0x80|((ch>>6)&0x3f);
      DST[3]=ch&0x3f;
    }
    return 4;
  }
  return -1;
}
