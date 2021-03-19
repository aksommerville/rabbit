/* rb_serial.h
 */
 
#ifndef RB_SERIAL_H
#define RB_SERIAL_H

// 0=incomplete, <0=malformed, 1..4=length
int rb_vlq_decode(int *dst,const void *src,int srcc);
int rb_vlq_encode(void *dst,int dsta,int src);

int rb_utf8_encode(void *dst,int dsta,int ch);

/* Structured encoder.
 */

struct rb_encoder {
  char *v;
  int c,a;
};

void rb_encoder_cleanup(struct rb_encoder *encoder);
int rb_encoder_require(struct rb_encoder *encoder,int addc);

int rb_encode_raw(struct rb_encoder *encoder,const void *src,int srcc);
int rb_encode_fmt(struct rb_encoder *encoder,const char *fmt,...);
int rb_encode_u8(struct rb_encoder *encoder,uint8_t v);
int rb_encode_u16(struct rb_encoder *encoder,uint16_t v);

int rb_encoder_insert(struct rb_encoder *encoder,int p,const void *src,int srcc);
int rb_encoder_insert_vlqlen(struct rb_encoder *encoder,int p);

#endif
