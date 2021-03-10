#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_event.h"
#include "rabbit/rb_serial.h"

/* Decode Meta or Sysex events in a file.
 */
 
static int rb_synth_event_decode_sysex(struct rb_synth_event *event,const uint8_t *src,int srcc) {
  int c,srcp;
  if ((srcp=rb_vlq_decode(&c,src,srcc))<1) return srcp;
  if (srcp>srcc-c) return 0;
  event->v=src+srcp;
  event->c=c;
  if ((event->c>0)&&(((uint8_t*)event->v)[event->c-1]==0xf7)) event->c--;
  return srcp;
}
 
static int rb_synth_event_decode_meta(struct rb_synth_event *event,const uint8_t *src,int srcc) {
  int srcp=0;
  if (srcp>=srcc) return 0;
  event->a=src[srcp++]; // type
  int c,err;
  if ((err=rb_vlq_decode(&c,src+srcp,srcc-srcp))<1) return err;
  srcp+=err;
  if (srcp>srcc-c) return 0;
  event->v=src+srcp;
  event->c=c;
  return srcp;
}

/* Decode from file.
 */
 
int rb_synth_event_decode_file(struct rb_synth_event *event,const void *src,int srcc,uint8_t *rstat) {
  if (srcc<1) return 0;
  const uint8_t *SRC=src;
  int srcp=0;
  #define REQ(c) if (srcp>srcc-c) return 0;
  
  uint8_t lead=SRC[srcp];
  if (lead&0x80) {
    *rstat=lead;
    srcp++;
  } else if (!((*rstat)&0x80)) {
    return -1;
  } else {
    lead=*rstat;
  }
    
  event->opcode=lead&0xf0;
  event->chid=lead&0x0f;
  
  switch (event->opcode) {
    case 0x80: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0x90: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; if (!event->b) { event->opcode=0x80; event->b=0x40; } break;
    case 0xa0: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0xb0: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0xc0: REQ(1) event->a=SRC[srcp++]; break;
    case 0xd0: REQ(1) event->a=SRC[srcp++]; break;
    case 0xe0: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0xf0: *rstat=0; switch (lead) {
        case 0xf0: 
        case 0xf7: {
            int err=rb_synth_event_decode_sysex(event,SRC+srcp,srcc-srcp);
            if (err<=0) return err;
            srcp+=err;
          } break;
        case 0xff: {
            int err=rb_synth_event_decode_meta(event,SRC+srcp,srcc-srcp);
            if (err<=0) return err;
            srcp+=err;
          } break;
        default: return -1;
      } break;
    default: return -1;
  }
  
  #undef REQ
  return srcp;
}

/* Decode from stream.
 */
 
int rb_synth_event_decode_stream(struct rb_synth_event *event,const void *src,int srcc) {
  if (srcc<1) return 0;
  const uint8_t *SRC=src;
  int srcp=0;
  #define REQ(c) if (srcp>srcc-c) return 0;
  
  uint8_t lead=SRC[srcp++];
  event->opcode=lead&0xf0;
  event->chid=lead&0x0f;
  switch (event->opcode) {
    case 0x80: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0x90: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0xa0: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0xb0: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0xc0: REQ(1) event->a=SRC[srcp++]; break;
    case 0xd0: REQ(1) event->a=SRC[srcp++]; break;
    case 0xe0: REQ(2) event->a=SRC[srcp++]; event->b=SRC[srcp++]; break;
    case 0xf0: event->opcode=lead; event->chid=RB_CHID_ALL; switch (lead) {
        case 0xf0: {
            event->v=SRC+srcp;
            event->c=0;
            while (1) {
              if (srcp>=srcc) return 0;
              if (SRC[srcp++]==0xf7) break;
              event->c++;
            }
          } break;
        default: break; // We are not handling realtime messages correctly.
      } break;
    default: return -1;
  }
  
  #undef REQ
  return srcp;
}
