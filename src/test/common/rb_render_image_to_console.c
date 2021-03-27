#include "test/rb_test.h"
#include "rabbit/rb_image.h"

/* Render one pixel.
 * https://en.wikipedia.org/wiki/ANSI_escape_code#24-bit
 *  ESC[ 38;5;<n> m Select foreground color
 *  ESC[ 48;5;<n> m Select background color
 *    0-  7:  standard colors (as in ESC [ 30–37 m)
 *    8- 15:  high intensity colors (as in ESC [ 90–97 m)
 *   16-231:  6 × 6 × 6 cube (216 colors): 16 + 36 × r + 6 × g + b (0 ≤ r, g, b ≤ 5)
 *  232-255:  grayscale from black to white in 24 steps
 */
 
static void rb_render_pixel_to_console(uint32_t argb) {
  uint8_t r=argb>>16,g=argb>>8,b=argb;
  int colorcode;
  if ((r==g)&&(g==b)) {
    uint8_t y=(r*24)>>8;
    if (r>=24) r=23;
    colorcode=232+y;
  } else {
    r=(r*6)>>8; if (r>5) r=5;
    g=(g*6)>>8; if (g>5) g=5;
    b=(b*6)>>8; if (b>5) b=5;
    colorcode=16+36*r+6*g+b;
  }
  fprintf(stderr,"\x1b[48;5;%dm  \x1b[0m",colorcode);
}

static void rb_render_alpha_to_console(uint32_t argb) {
  uint8_t a=argb>>24;
  a=(a*24)>>8;
  if (a>=24) a=23;
  a+=232;
  fprintf(stderr,"\x1b[48;5;%dm  \x1b[0m",a);
}

/* Render an image to stderr with VT escapes.
 */
 
void rb_render_image_to_console(const struct rb_image *image) {
  fprintf(stderr,"%s... %dx%d, alphamode=%d\n",__func__,image->w,image->h,image->alphamode);
  
  const int wlimit=64,hlimit=64;
  int w=image->w;
  if (w>wlimit) w=wlimit;
  int h=image->h;
  if (h>hlimit) h=hlimit;
  if ((w<image->w)||(h<image->h)) {
    fprintf(stderr,"Cropping to %dx%d\n",w,h);
  }
  
  const uint32_t *src=image->pixels;
  int yi=h;
  for (;yi-->0;src+=image->w) {
    fprintf(stderr,"  ");
    const uint32_t *p=src;
    int xi=w;
    for (;xi-->0;p++) {
      rb_render_pixel_to_console(*p);
    }
    fprintf(stderr,"\n");
  }
  
  fprintf(stderr,"----- end of image dump\n");
}

/* Render just the alpha channel of an image to stderr with VT escapes.
 */
 
void rb_render_image_alpha_to_console(const struct rb_image *image) {
  fprintf(stderr,"%s... %dx%d, alphamode=%d\n",__func__,image->w,image->h,image->alphamode);
  
  const int wlimit=64,hlimit=64;
  int w=image->w;
  if (w>wlimit) w=wlimit;
  int h=image->h;
  if (h>hlimit) h=hlimit;
  if ((w<image->w)||(h<image->h)) {
    fprintf(stderr,"Cropping to %dx%d\n",w,h);
  }
  
  const uint32_t *src=image->pixels;
  int yi=h;
  for (;yi-->0;src+=image->w) {
    fprintf(stderr,"  ");
    const uint32_t *p=src;
    int xi=w;
    for (;xi-->0;p++) {
      rb_render_alpha_to_console(*p);
    }
    fprintf(stderr,"\n");
  }
  
  fprintf(stderr,"----- end of image dump\n");
}
