#include "rb_png.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Cleanup image.
 */

void rb_png_image_cleanup(struct rb_png_image *image) {
  if (!image) return;
  if (image->pixels) free(image->pixels);
  if (image->message) free(image->message);
  memset(image,0,sizeof(struct rb_png_image));
}

/* Decode header only.
 */
 
int rb_png_decode_header(
  struct rb_png_image *image,
  const void *src,int srcc
) {
  if (!src) return -1;
  if (srcc<16+13) return -1;
  if (memcmp(src,
    "\x89PNG\r\n\x1a\n"
    "\0\0\0\x0d""IHDR",16
  )) return -1;
  const uint8_t *SRC=src;
  
  image->w=(SRC[16]<<24)|(SRC[17]<<16)|(SRC[18]<<8)|SRC[19];
  if ((image->w<1)||(image->w>RB_PNG_SIZE_LIMIT)) return -1;
  image->h=(SRC[20]<<24)|(SRC[21]<<16)|(SRC[22]<<8)|SRC[23];
  if ((image->h<1)||(image->h>RB_PNG_SIZE_LIMIT)) return -1;
  image->depth=SRC[24];
  if (image->depth!=8) return -1; // 1,2,4,16 legal but not to us
  switch (image->colortype=SRC[25]) {
    case 0: break;
    case 2: break;
    // 3 legal but not to us
    case 4: break;
    case 6: break;
    default: return -1;
  }
  if (image->compression=SRC[26]) return -1;
  if (image->filter=SRC[27]) return -1;
  if (image->interlace=SRC[28]) return -1; // 1 legal but not to us
  
  return 0;
}

/* Set error message.
 */
 
int rb_png_fail(struct rb_png_image *image,const char *fmt,...) {
  if (!image->messagec&&fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char tmp[256];
    int tmpc=vsnprintf(tmp,sizeof(tmp),fmt,vargs);
    if ((tmpc>0)&&(tmpc<sizeof(tmp))) {
      char *nv=malloc(tmpc+1);
      if (nv) {
        if (image->message) free(image->message);
        image->message=nv;
        image->messagec=tmpc;
        memcpy(nv,tmp,tmpc+1);
      }
    }
  }
  return -1;
}
