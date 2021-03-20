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

/* Framebuffer is a packed 32-bit RGB image of fixed size 256x144.
 * The high 8 bits of each pixel are ignored.
 ******************************************************/

#define RB_FB_W 256
#define RB_FB_H 144
#define RB_FB_SIZE_BYTES (RB_FB_W*RB_FB_H*4)

struct rb_framebuffer {
  uint32_t v[RB_FB_W*RB_FB_H];
};

void rb_framebuffer_clear(struct rb_framebuffer *fb,uint32_t xrgb);

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

/* Rendering.
 ********************************************************/

/* We check bounds and reject if invalid -- we do not attempt to correct them.
 * If (xform) swaps axes, (w,h) refer to the source image. Output dimensions will be (h,w).
 * Override per-pixel transfer with a custom (blend) function. Beware, that's expensive.
 * (blend) is called for every pixel if input alphamode is BLEND or OPAQUE.
 * For COLORKEY and DISCRETE images, we only call for pixels we think generically are opaque.
 */
int rb_framebuffer_blit(
  struct rb_framebuffer *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
);

int rb_framebuffer_blit_safe(
  struct rb_framebuffer *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
);

/* Invalid bounds may segfault.
 * You must rb_framebuffer_check_bounds() first.
 */
void rb_framebuffer_blit_unchecked(
  struct rb_framebuffer *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
);

/* Adjust (dstx,dsty,srcx,srcy,w,h) if needed to keep all in bounds.
 * Returns >0 if the final bounds are valid.
 */
int rb_framebuffer_check_bounds(
  const struct rb_framebuffer *dst,int *dstx,int *dsty,
  const struct rb_image *src,int *srcx,int *srcy,
  int *w,int *h,
  uint8_t xform
);

/* Nudge the content of (image) by (dx,dy) pixels.
 * This leaves a column or row of duplicated pixels on one side.
 */
int rb_image_scroll(struct rb_image *image,int dx,int dy);

#endif
