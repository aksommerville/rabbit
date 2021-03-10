/* rb_signal.h
 */
 
#ifndef RB_SIGNAL_H
#define RB_SIGNAL_H

#include <string.h>
#include <stdint.h>

typedef float rb_sample_t;

rb_sample_t rb_rate_from_noteid(uint8_t noteid);

static inline void rb_signal_set_s(
  rb_sample_t *v,int c,rb_sample_t a
) {
  if (c<1) return;
  v[0]=a;
  int havec=1;
  while (havec<c) {
    int cpc=c-havec;
    if (cpc>havec) cpc=havec;
    memcpy(v+havec,v,cpc*sizeof(rb_sample_t));
    havec+=cpc;
  }
}

static inline void rb_signal_add_s(
  rb_sample_t *v,int c,rb_sample_t a
) {
  for (;c-->0;v++) (*v)+=a;
}

static inline void rb_signal_add_v(
  rb_sample_t *v,int c,const rb_sample_t *a
) {
  for (;c-->0;v++,a++) (*v)+=(*a);
}

static inline void rb_signal_mlt_s(
  rb_sample_t *v,int c,rb_sample_t a
) {
  for (;c-->0;v++) (*v)*=a;
}

static inline void rb_signal_mlt_v(
  rb_sample_t *v,int c,const rb_sample_t *a
) {
  for (;c-->0;v++,a++) (*v)*=(*a);
}

static inline void rb_signal_quantize(
  int16_t *dst,const rb_sample_t *src,int c,int16_t qlevel
) {
  for (;c-->0;dst++,src++) {
    if (*src>=1.0f) *dst=qlevel;
    else if (*src<=-1.0f) *dst=-qlevel;
    else *dst=(*src)*qlevel;
  }
}

static inline void rb_signal_quantize_unchecked(
  int16_t *dst,const rb_sample_t *src,int c,int16_t qlevel
) {
  for (;c-->0;dst++,src++) {
    *dst=(*src)*qlevel;
  }
}

#endif
