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
