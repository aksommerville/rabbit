#include "test/rb_test.h"
#include "rabbit/rb_image.h"
#include "lib/image/rb_image_obj.c"
#include "lib/image/rb_image_blit.c"

/* Generate 4x4-pixel test images.
 */
 
#undef RB_ERROR_RETURN_VALUE
#define RB_ERROR_RETURN_VALUE 0
 
// Top row grays, then red, green blue. Contains meaningless alpha.
static struct rb_image *generate_opaque() {
  struct rb_image *image=rb_image_new(4,4);
  RB_ASSERT(image)
  image->alphamode=RB_ALPHAMODE_OPAQUE;
  image->pixels[ 0]=0x00000000;
  image->pixels[ 1]=0x55ffffff;
  image->pixels[ 2]=0xaaaaaaaa;
  image->pixels[ 3]=0xff555555;
  image->pixels[ 4]=0x00ff0000;
  image->pixels[ 5]=0x55ff0000;
  image->pixels[ 6]=0xaaff0000;
  image->pixels[ 7]=0xffff0000;
  image->pixels[ 8]=0x0000ff00;
  image->pixels[ 9]=0x5500ff00;
  image->pixels[10]=0xaa00ff00;
  image->pixels[11]=0xff00ff00;
  image->pixels[12]=0x000000ff;
  image->pixels[13]=0x550000ff;
  image->pixels[14]=0xaa0000ff;
  image->pixels[15]=0xff0000ff;
  return image;
}

// Dark to light horizontally, transparent to opaque vertically.
static struct rb_image *generate_alpha() {
  struct rb_image *image=rb_image_new(4,4);
  RB_ASSERT(image)
  image->alphamode=RB_ALPHAMODE_BLEND;
  image->pixels[ 0]=0x00000000;
  image->pixels[ 1]=0x00555555;
  image->pixels[ 2]=0x00aaaaaa;
  image->pixels[ 3]=0x00ffffff;
  image->pixels[ 4]=0x55004000;
  image->pixels[ 5]=0x55008000;
  image->pixels[ 6]=0x5500c000;
  image->pixels[ 7]=0x5500ff00;
  image->pixels[ 8]=0xaa000040;
  image->pixels[ 9]=0xaa000080;
  image->pixels[10]=0xaa0000c0;
  image->pixels[11]=0xaa0000ff;
  image->pixels[12]=0xff400000;
  image->pixels[13]=0xff800000;
  image->pixels[14]=0xffc00000;
  image->pixels[15]=0xffff0000;
  return image;
}

// Solid half-gray with a checkerboard of intermediate alphas
static struct rb_image *generate_alpha_pattern() {
  struct rb_image *image=rb_image_new(4,4);
  RB_ASSERT(image)
  image->alphamode=RB_ALPHAMODE_BLEND;
  image->pixels[ 0]=0x55808080;
  image->pixels[ 1]=0xaa808080;
  image->pixels[ 2]=0x55808080;
  image->pixels[ 3]=0xaa808080;
  image->pixels[ 4]=0xaa808080;
  image->pixels[ 5]=0x55808080;
  image->pixels[ 6]=0xaa808080;
  image->pixels[ 7]=0x55808080;
  image->pixels[ 8]=0x55808080;
  image->pixels[ 9]=0xaa808080;
  image->pixels[10]=0x55808080;
  image->pixels[11]=0xaa808080;
  image->pixels[12]=0xaa808080;
  image->pixels[13]=0x55808080;
  image->pixels[14]=0xaa808080;
  image->pixels[15]=0x55808080;
  return image;
}
 
#undef RB_ERROR_RETURN_VALUE
#define RB_ERROR_RETURN_VALUE -1

/* Control: Blit to an opaque image.
 */
 
static int blit_to_opaque() {
  struct rb_image *dst=generate_opaque();
  struct rb_image *src=generate_alpha();
  RB_ASSERT(dst&&src)
  
  //rb_render_image_to_console(dst);
  //rb_render_image_to_console(src);
  
  rb_image_blit_unchecked(dst,0,0,src,0,0,4,4,0,0,0);
  //rb_render_image_to_console(dst);
  
  // Top row must not have changed.
  RB_ASSERT_INTS(dst->pixels[0],0x00000000)
  RB_ASSERT_INTS(dst->pixels[1],0x55ffffff)
  RB_ASSERT_INTS(dst->pixels[2],0xaaaaaaaa)
  RB_ASSERT_INTS(dst->pixels[3],0xff555555)
  
  // Bottom row must match (src) exactly.
  RB_ASSERT_NOT(memcmp(dst->pixels+12,src->pixels+12,16))
  
  rb_image_del(dst);
  rb_image_del(src);
  return 0;
}

/* Blit one BLEND image onto another -- alphas should add and saturate.
 */
 
static int blit_to_alpha() {
  struct rb_image *dst=generate_alpha_pattern();
  struct rb_image *src=generate_alpha();
  RB_ASSERT(dst&&src)
  
  //rb_render_image_to_console(dst);
  //rb_render_image_alpha_to_console(dst);
  //rb_render_image_to_console(src);
  //rb_render_image_alpha_to_console(src);
  
  rb_image_blit_unchecked(dst,0,0,src,0,0,4,4,0,0,0);
  //rb_render_image_to_console(dst);
  //rb_render_image_alpha_to_console(dst);
  
  // Confirm that alpha added and saturated.
  // Only need to look at the first two columns.
  RB_ASSERT_INTS(dst->pixels[ 0]>>24,0x55,"0,0")
  RB_ASSERT_INTS(dst->pixels[ 1]>>24,0xaa,"1,0")
  RB_ASSERT_INTS(dst->pixels[ 4]>>24,0xff,"0,1") // <-- ff exactly
  RB_ASSERT_INTS(dst->pixels[ 5]>>24,0xaa,"1,1") // <-- 55+55
  RB_ASSERT_INTS(dst->pixels[ 8]>>24,0xff,"0,2") // <-- also ff exactly
  RB_ASSERT_INTS(dst->pixels[ 9]>>24,0xff,"1,2") // <-- saturated...
  RB_ASSERT_INTS(dst->pixels[12]>>24,0xff,"0,3")
  RB_ASSERT_INTS(dst->pixels[13]>>24,0xff,"1,3")
  
  rb_image_del(dst);
  rb_image_del(src);
  return 0;
}

/* Blit an OPAQUE image onto a COLORKEY one.
 * It's important that a zero source pixel turn into something nonzero.
 */
 
static int blit_to_colorkey() {
  struct rb_image *dst=rb_image_new(2,2);
  struct rb_image *src=rb_image_new(2,2);
  RB_ASSERT(dst&&src)
  
  dst->alphamode=RB_ALPHAMODE_COLORKEY;
  dst->pixels[0]=0x80808080;
  dst->pixels[1]=0x80808080;
  dst->pixels[2]=0x80808080;
  dst->pixels[3]=0x80808080;
  src->alphamode=RB_ALPHAMODE_OPAQUE;
  // These four source pixels all have zero in the high byte, but must behave as if it were one.
  src->pixels[0]=0x00000000;
  src->pixels[1]=0x00ff0000;
  src->pixels[2]=0x0000ff00;
  src->pixels[3]=0x000000ff;
  
  rb_image_blit_unchecked(dst,0,0,src,0,0,2,2,0,0,0);
  
  //rb_render_image_to_console(dst);
  //rb_render_image_alpha_to_console(dst);
  
  // All output pixels must be nonzero, even the one that copied from an input zero.
  RB_ASSERT(dst->pixels[0])
  RB_ASSERT(dst->pixels[1])
  RB_ASSERT(dst->pixels[2])
  RB_ASSERT(dst->pixels[3])
  
  // RGB portion of output must match input exactly.
  RB_ASSERT_INTS(dst->pixels[0]&0x00ffffff,src->pixels[0]&0x00ffffff)
  RB_ASSERT_INTS(dst->pixels[1]&0x00ffffff,src->pixels[1]&0x00ffffff)
  RB_ASSERT_INTS(dst->pixels[2]&0x00ffffff,src->pixels[2]&0x00ffffff)
  RB_ASSERT_INTS(dst->pixels[3]&0x00ffffff,src->pixels[3]&0x00ffffff)
  
  rb_image_del(dst);
  rb_image_del(src);
  return 0;
}

/* TOC
 */
 
int main(int argc,char **argv) {
  RB_UTEST(blit_to_opaque)
  RB_UTEST(blit_to_alpha)
  RB_UTEST(blit_to_colorkey)
  return 0;
}
