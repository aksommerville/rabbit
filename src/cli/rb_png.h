/* rb_png.h
 * Bare bones PNG decoder.
 * We do not support:
 *  - encoding
 *  - interlaced images
 *  - channel sizes other than 8
 *  - color tables
 *  - sizes beyond 1024x1024
 * IHDR must be the first chunk and must be exactly 13 bytes long.
 * Unexpected chunks (including etc PLTE, tRNS) are quietly ignored.
 */
 
#ifndef RB_PNG_H
#define RB_PNG_H

#include <stdint.h>

#define RB_PNG_SIZE_LIMIT 1024

struct rb_png_image {
  void *pixels;
  int pixelslen;
  int w,h;
  int stride;
  uint8_t depth;
  uint8_t colortype;
  uint8_t compression;
  uint8_t filter;
  uint8_t interlace;
  char *message;
  int messagec;
};

void rb_png_image_cleanup(struct rb_png_image *image);

/* Read just the IHDR chunk and populate everything except (pixels).
 * Errors never set (message).
 * If you want details after a failure, call rb_png_decode() to get a message.
 * This is very cheap, no problem to do it redundantly.
 * No need to clean up (image) after.
 */
int rb_png_decode_header(
  struct rb_png_image *image,
  const void *src,int srcc
);

/* Drop any content previously in (image) and decode the complete file at (src) into it.
 * Any failure, we will attempt to set (image->message).
 * We may fail after allocating some content in (image); please clean up pass or fail.
 */
int rb_png_decode(
  struct rb_png_image *image,
  const void *src,int srcc
);

static inline int rb_png_colortype_is_gray(uint8_t colortype) {
  if (colortype==0) return 1;
  if (colortype==4) return 1;
  return 0;
}

static inline int rb_png_colortype_is_rgb(uint8_t colortype) {
  if (colortype==2) return 1;
  if (colortype==6) return 1;
  return 0;
}

static inline int rb_png_colortype_has_alpha(uint8_t colortype) {
  if (colortype==4) return 1;
  if (colortype==6) return 1;
  return 0;
}

int rb_png_fail(struct rb_png_image *image,const char *fmt,...);

#endif
