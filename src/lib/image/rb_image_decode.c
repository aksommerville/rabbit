#include "rabbit/rb_internal.h"
#include "rabbit/rb_image.h"

/* Copy one row from a given format.
 */
 
static void rb_image_decode_row_RGBA(uint32_t *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=4) *dst=(src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
}
 
static void rb_image_decode_row_RGB(uint32_t *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=3) *dst=0xff000000|(src[0]<<16)|(src[1]<<8)|src[2];
}
 
static void rb_image_decode_row_RGBCK(uint32_t *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src+=3) {
    *dst=(src[0]<<16)|(src[1]<<8)|src[2];
    if (*dst) (*dst)|=0xff000000;
  }
}
 
static void rb_image_decode_row_A8(uint32_t *dst,const uint8_t *src,int c) {
  for (;c-->0;dst++,src++) *dst=(*src)<<24;
}
 
static void rb_image_decode_row_A1(uint32_t *dst,const uint8_t *src,int c) {
  uint8_t mask=0x80;
  for (;c-->0;dst++) {
    if ((*src)&mask) *dst=0xff000000;
    if (!(mask>>=1)) {
      src++;
      mask=0x80;
    }
  }
}

/* Decode, main entry point.
 */
 
struct rb_image *rb_image_new_decode(const void *src,int srcc) {
  if (!src||(srcc<4)) return 0;
  const uint8_t *SRC=src;
  int srcp=0;
  
  int format=SRC[srcp++];
  int w=((SRC[srcp]<<4)|(SRC[srcp+1]>>4))+1;
  int h=(((SRC[srcp+1]&0x0f)<<8)|SRC[srcp+2])+1;
  srcp+=3;
  
  int stride=w;
  void (*cp)(uint32_t *dst,const uint8_t *src,int w);
  switch (format) {
    case RB_IMAGE_FORMAT_RGBA: stride<<=2; cp=rb_image_decode_row_RGBA; break;
    case RB_IMAGE_FORMAT_RGB: stride*=3; cp=rb_image_decode_row_RGB; break;
    case RB_IMAGE_FORMAT_RGBCK: stride*=3; cp=rb_image_decode_row_RGBCK; break;
    case RB_IMAGE_FORMAT_A8: cp=rb_image_decode_row_A8; break;
    case RB_IMAGE_FORMAT_A1: stride=(w+7)>>3; cp=rb_image_decode_row_A1; break;
    default: return 0;
  }
  int expect=stride*h;
  if (srcp+expect<srcc) return 0;
  
  struct rb_image *image=rb_image_new(w,h);
  if (!image) return 0;
  
  uint32_t *dstrow=image->pixels;
  int i=h; for (;i-->0;dstrow+=image->w,srcp+=stride) {
    cp(dstrow,SRC+srcp,w);
  }
  
  switch (format) {
    case RB_IMAGE_FORMAT_RGBA: image->alphamode=RB_ALPHAMODE_BLEND; break;
    case RB_IMAGE_FORMAT_RGB: image->alphamode=RB_ALPHAMODE_OPAQUE; break;
    case RB_IMAGE_FORMAT_RGBCK: image->alphamode=RB_ALPHAMODE_COLORKEY; break;
    case RB_IMAGE_FORMAT_A8: image->alphamode=RB_ALPHAMODE_BLEND; break;
    case RB_IMAGE_FORMAT_A1: image->alphamode=RB_ALPHAMODE_COLORKEY; break;
  }
  
  return image;
}
