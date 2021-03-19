#include "rabbit/rb_internal.h"
#include "rabbit/rb_serial.h"
#include <stdarg.h>

/* Cleanup.
 */
 
void rb_encoder_cleanup(struct rb_encoder *encoder) {
  if (encoder->v) free(encoder->v);
  memset(encoder,0,sizeof(struct rb_encoder));
}

/* Grow buffer.
 */
 
int rb_encoder_require(struct rb_encoder *encoder,int addc) {
  if (addc<1) return 0;
  if (!encoder) return -1;
  if (encoder->c>INT_MAX-addc) return -1;
  int na=encoder->c+addc;
  if (na<=encoder->a) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  void *nv=realloc(encoder->v,na);
  if (!nv) return -1;
  encoder->v=nv;
  encoder->a=na;
  return 0;
}

/* Append content.
 */
 
int rb_encode_raw(struct rb_encoder *encoder,const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (rb_encoder_require(encoder,srcc)<0) return -1;
  memcpy(encoder->v+encoder->c,src,srcc);
  encoder->c+=srcc;
  return 0;
}

int rb_encode_fmt(struct rb_encoder *encoder,const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  va_list vargs;
  while (1) {
    va_start(vargs,fmt);
    int err=vsnprintf(encoder->v+encoder->c,encoder->a-encoder->c,fmt,vargs);
    if ((err<0)||(err==INT_MAX)) return -1;
    if (encoder->c<encoder->a-err) { // sic <
      encoder->c+=err;
      return 0;
    }
    if (rb_encoder_require(encoder,err+1)<0) return -1;
  }
}

int rb_encode_u8(struct rb_encoder *encoder,uint8_t v) {
  if (rb_encoder_require(encoder,1)<0) return -1;
  encoder->v[encoder->c++]=v;
  return 0;
}

int rb_encode_u16(struct rb_encoder *encoder,uint16_t v) {
  if (rb_encoder_require(encoder,2)<0) return -1;
  encoder->v[encoder->c++]=v>>8;
  encoder->v[encoder->c++]=v;
  return 0;
}

/* Insert anything.
 */
 
int rb_encoder_insert(struct rb_encoder *encoder,int p,const void *src,int srcc) {
  if ((p<0)||(p>encoder->c)) return -1;
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (((char*)src)[srcc]) srcc++; }
  if (rb_encoder_require(encoder,srcc)<0) return -1;
  memmove(encoder->v+p+srcc,encoder->v+p,encoder->c-p);
  memcpy(encoder->v+p,src,srcc);
  encoder->c+=srcc;
  return 0;
}

/* Insert length to end.
 */

int rb_encoder_insert_vlqlen(struct rb_encoder *encoder,int p) {
  if ((p<0)||(p>encoder->c)) return -1;
  int len=encoder->c-p;
  char tmp[4];
  int tmpc=rb_vlq_encode(tmp,4,len);
  if (tmpc<1) return -1;
  if (rb_encoder_require(encoder,tmpc)<0) return -1;
  memmove(encoder->v+p+tmpc,encoder->v+p,len);
  memcpy(encoder->v+p,tmp,tmpc);
  encoder->c+=tmpc;
  return 0;
}
