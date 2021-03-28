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

/* Primitives.
 ****************************************************************/
 
void rb_image_fill_rect(struct rb_image *image,int x,int y,int w,int h,uint32_t argb);

// Writes alpha verbatim, no blending.
void rb_image_fill_circle(struct rb_image *image,int x,int y,int radius,uint32_t argb);

void rb_image_circle_alpha_gradient(
  struct rb_image *image,
  int x,int y,
  int radius,int gradius,
  uint8_t abase
);

// (a) if (d<=0), (b) if (d>=1), otherwise somewhere in between.
// Alpha blends like any other channel.
uint32_t rb_argb_blend(uint32_t a,uint32_t b,double d);

/* Replace every pixel in (image) with either 0, (main), or (edge).
 * Everything opaque gets (main), adjacent to an opaque pixel gets (edge), and otherwise zero.
 * For colorizing and outlining text, mostly.
 * You may provide (dst), in which case (src) is read-only.
 * Otherwise we create a scratch image and copy it back to (src) at the end.
 */
void rb_image_trace_edges(struct rb_image *dst,struct rb_image *src,uint32_t main,uint32_t edge);

/* Replace each pixel with either (opaque) or (transparent).
 * BLEND and DISCRETE images behave the same way, and OPAQUE fills entirely with (opaque).
 */
void rb_image_replace_by_alpha(struct rb_image *image,uint32_t opaque,uint32_t transparent);

// 0=black ... 0xff=noop
void rb_image_darken(struct rb_image *image,uint8_t brightness);

/* Lighting.
 * For fade-to-black or spotlight effects.
 * Framebuffer gets darkened by a uniform amount, except for circles around designated light sources.
 * This is expensive! Avoid if you don't need it, and use as few lights as possible.
 ***************************************************************/
 
struct rb_lights {
  
  /* 0x00: Full black. Underlying image is irrevocably destroyed.
   * 0xff: Noop.
   */
  uint8_t bg;
  
  struct rb_light {
    int id; // Constant
    int x,y; // Position in world space.
    
    /* (radius) is the fully illuminated inner circle, and (gradius) the additional extent of fading out.
     * Both must be >=0. If both are zero, the light is noop.
     * I do recommend using gradius, even just 1 softens the circles' edges nicely.
     * Gradients follow a quadratic curve, not linear.
     * That's for computational simplicity, but also I think it looks better.
     */
    int radius;
    int gradius;
    
    // Remainder for internal use:
    int visible;
    int ya,yz;
    double inner,outer;
    double adjust;
  } *lightv;
  int lightc,lighta;
  struct rb_image *scratch;
};

void rb_lights_cleanup(struct rb_lights *lights);

/* Add a light with id 0 to make up an unused id.
 * Lights are not guaranteed to remain in the same order after drawing; 'get' fresh when you need it.
 */
struct rb_light *rb_lights_add(struct rb_lights *lights,int id);
struct rb_light *rb_lights_get(const struct rb_lights *lights,int id);
int rb_lights_remove(struct rb_lights *lights,int id);
int rb_lights_clear(struct rb_lights *lights);

/* Draw (lights->bgargb) over (dst), except where it intersects one of the lights.
 * (scrollx,scrolly) is subtracted from each light position, normally you get this from vmgr.
 * We only accept OPAQUE images.
 */
int rb_lights_draw(
  struct rb_image *dst,
  struct rb_lights *lights,
  int scrollx,int scrolly
);

#endif
