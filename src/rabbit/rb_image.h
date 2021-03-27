/* rb_image.h
 */
 
#ifndef RB_IMAGE_H
#define RB_IMAGE_H

/* There are 8 transforms when blitting,
 * which are neatly summarized in 3 bits.
 * 1 each to reverse each axis, and another to swap axes.
 * Reversal happens before swapping.
 * A transformed image will always have the same top-left corner in the output.
 */
#define RB_XFORM_NONE    0
#define RB_XFORM_XREV    1
#define RB_XFORM_YREV    2
#define RB_XFORM_SWAP    4

/* Renderable image is 32-bit ARGB. (Alpha in the most-significant byte)
 * Dimensions are fixed at construction.
 ****************************************************/
 
#define RB_IMAGE_SIZE_LIMIT 4096

#define RB_ALPHAMODE_BLEND    0 /* Mix colors continuously, default. */
#define RB_ALPHAMODE_COLORKEY 1 /* Only pixel 0x00000000 is transparent, all others opaque. */
#define RB_ALPHAMODE_DISCRETE 2 /* Alpha >=0x80 is 100% opaque, <=0x7f is 100% transparent. */
#define RB_ALPHAMODE_OPAQUE   3 /* No alpha at all. */
 
struct rb_image {
  int refc;
  int w,h;
  int alphamode;
  uint32_t pixels[];
};

struct rb_image *rb_image_new(int w,int h);
void rb_image_del(struct rb_image *image);
int rb_image_ref(struct rb_image *image);

#define RB_FB_W 256
#define RB_FB_H 144
#define RB_FB_SIZE_BYTES (RB_FB_W*RB_FB_H*4)
struct rb_image *rb_framebuffer_new();

/* Our images are encoded in a pretty simple format for easy loading.
 * Starts with a 32-bit header:
 *   ff000000 format
 *   00fff000 width-1
 *   00000fff height-1
 * Then pixels, rows padded to one byte.
 */
struct rb_image *rb_image_new_decode(const void *src,int srcc);
#define RB_IMAGE_FORMAT_RGBA    0x01 /* 32-bit RGBA */
#define RB_IMAGE_FORMAT_RGB     0x02 /* Opaque 24-bit RGB */
#define RB_IMAGE_FORMAT_RGBCK   0x03 /* 24-bit RGB and pure black is transparent */
#define RB_IMAGE_FORMAT_A8      0x04 /* 8-bit alpha */
#define RB_IMAGE_FORMAT_A1      0x05 /* 1-bit alpha */

/* Rendering.
 ********************************************************/
 
int rb_image_clear(struct rb_image *image,uint32_t argb);

/* We check bounds and reject if invalid -- we do not attempt to correct them.
 * If (xform) swaps axes, (w,h) refer to the source image. Output dimensions will be (h,w).
 * Override per-pixel transfer with a custom (blend) function. Beware, that's expensive.
 * (blend) is called for every pixel if input alphamode is BLEND or OPAQUE.
 * For COLORKEY and DISCRETE images, we only call for pixels we think generically are opaque.
 */
int rb_image_blit(
  struct rb_image *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
);

int rb_image_blit_safe(
  struct rb_image *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
);

/* Invalid bounds may segfault.
 * You must rb_image_check_bounds() first.
 */
void rb_image_blit_unchecked(
  struct rb_image *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
);

/* Adjust (dstx,dsty,srcx,srcy,w,h) if needed to keep all in bounds.
 * Returns >0 if the final bounds are valid.
 */
int rb_image_check_bounds(
  const struct rb_image *dst,int *dstx,int *dsty,
  const struct rb_image *src,int *srcx,int *srcy,
  int *w,int *h,
  uint8_t xform
);

/* Nudge the content of (image) by (dx,dy) pixels.
 * This leaves a column or row of duplicated pixels on one side.
 */
int rb_image_scroll(struct rb_image *image,int dx,int dy);

#endif
