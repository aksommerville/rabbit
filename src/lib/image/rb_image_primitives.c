#include "rabbit/rb_internal.h"
#include "rabbit/rb_image.h"
#include <math.h>
#if RB_ARCH==RB_ARCH_macos
  #include <machine/endian.h>
#else
  #include <endian.h>
#endif

/* Blend colors.
 */
 
uint32_t rb_argb_blend(uint32_t a,uint32_t b,double d) {
  if (d<=0.0) return a;
  if (d>=1.0) return b;
  double inv=1.0-d;
  int dsta=(a>>24)*inv+(b>>24)*d;
  int dstr=((a>>16)&0xff)*inv+((b>>16)&0xff)*d;
  int dstg=((a>>8)&0xff)*inv+((b>>8)&0xff)*d;
  int dstb=(a&0xff)*inv+(b&0xff)*d;
  if (dsta<0) dsta=0; else if (dsta>0xff) dsta=0xff;
  if (dstr<0) dstr=0; else if (dstr>0xff) dstr=0xff;
  if (dstg<0) dstg=0; else if (dstg>0xff) dstg=0xff;
  if (dstb<0) dstb=0; else if (dstb>0xff) dstb=0xff;
  return (dsta<<24)|(dstr<<16)|(dstg<<8)|dstb;
}

/* Fill rectangle.
 */
 
void rb_image_fill_rect(struct rb_image *image,int x,int y,int w,int h,uint32_t argb) {
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>image->w-w) w=image->w-x;
  if (y>image->h-h) h=image->h-y;
  if ((w<1)||(h<1)) return;
  uint32_t *row=image->pixels+y*image->w+x;
  int yi=h;
  for (;yi-->0;row+=image->w) {
    uint32_t *p=row;
    int xi=w;
    for (;xi-->0;p++) *p=argb;
  }
}

/* Fill row.
 */
 
void rb_image_fill_row(struct rb_image *image,int x,int y,int w,uint32_t argb) {
  if (y<0) return;
  if (y>=image->h) return;
  if (x<0) { w+=x; x=0; }
  if (x>image->w-w) w=image->w-x;
  uint32_t *p=image->pixels+y*image->w+x;
  for (;w-->0;p++) *p=argb;
}

/* Fill circle.
 */
 
void rb_image_fill_circle(struct rb_image *image,int x,int y,int radius,uint32_t argb) {
  int radius2=radius*radius;
  int dy=0,dy2=0,dx=radius-1,dx2=dx*dx;
  int i=radius;
  while (i-->0) {
  
    rb_image_fill_row(image,x-dx,y-dy,dx+dx,argb);
    rb_image_fill_row(image,x-dx,y+dy,dx+dx,argb);
  
    dy2=dy2+dy+dy+1;
    dy++;
    while (dx2+dy2>radius2) {
      dx2=dx2-dx-dx-1;
      dx--;
      if (dx<1) return;
    }
  }
}

/* Fill circle with gradient edge, alpha only.
XXX Need to figure what I want, then ask for it, in that order. Come back later.
 */
 
void rb_image_circle_alpha_gradient(
  struct rb_image *image,
  int x,int y,
  int radius,int gradius,
  uint8_t abase
) {

  // Visit every pixel that could be within the outer circle.
  int x0=x-radius-gradius;
  int y0=y-radius-gradius;
  int w=radius+radius+gradius+gradius;
  int h=w;
  if (x0<0) { w+=x0; x0=0; }
  if (y0<0) { h+=y0; y0=0; }
  if (x0>image->w-w) w=image->w-x0;
  if (y0>image->h-h) h=image->h-y0;
  if ((w<1)||(h<1)) return;
  
  int mdinner=radius; // manhattan distance < mdinner, use argb verbatim
  int mdouter=M_SQRT2*(radius+gradius)+1; // > mdouter, discard
  double fradius=radius;
  double gscale=1.0/gradius;
  
  uint32_t *row=image->pixels+y0*image->w+x0;
  int yi=h,py=y0;
  for (;yi-->0;py++,row+=image->w) {
    uint32_t *p=row;
    int xi=w,px=x0;
    for (;xi-->0;px++,p++) {
      
    
      // Take the Manhattan distance to center.
      int dx=px-x; if (dx<0) dx=-dx;
      int dy=py-y; if (dy<0) dy=-dy;
      int md=dx+dy;
      if (md>=mdouter) continue;
      if (md<=mdinner) {
        (*p)&=0x00ffffff;
        continue;
      }
      
      // Take true distance.
      double d=sqrt(dx*dx+dy*dy);
      d=(d-fradius)*gscale;
      if (d>=1.0) continue;
      if (d<=0.0) {
        (*p)&=0x00ffffff;
        continue;
      }
      
      // Blend.
      d=1.0-d;
      uint8_t pva=(*p)>>24;
      int a=pva-d*abase;
      if (a<0) a=0;
      //if (a>pva) continue;
      *p=((*p)&0x00ffffff)|(a<<24);
    }
  }
}

/* Trace edges.
 */
 
static void rb_image_trace_edges_1pn(uint32_t *dst,const uint32_t *src,int w,uint32_t main,uint32_t edge) {
  const uint32_t *srcpv=src-w,*srcnx=src+w;
  
  if (src[0]&0xff000000) *dst=main;
  else if ((w>1)&&(src[1]&0xff000000)) *dst=edge;
  else if (srcpv[0]&0xff000000) *dst=edge;
  else if (srcnx[0]&0xff000000) *dst=edge;
  else *dst=0;
  dst++; src++; srcpv++; srcnx++;
  
  int xi=w-2;
  for (;xi-->0;dst++,src++,srcpv++,srcnx++) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else if (src[1]&0xff000000) *dst=edge;
    else if (srcpv[0]&0xff000000) *dst=edge;
    else if (srcnx[0]&0xff000000) *dst=edge;
    else *dst=0;
  }
  
  if (w>1) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else if (srcpv[0]&0xff000000) *dst=edge;
    else if (srcnx[0]&0xff000000) *dst=edge;
    else *dst=0;
  }
}
 
static void rb_image_trace_edges_1p(uint32_t *dst,const uint32_t *src,int w,uint32_t main,uint32_t edge) {
  const uint32_t *srcpv=src-w;
  
  if (src[0]&0xff000000) *dst=main;
  else if ((w>1)&&(src[1]&0xff000000)) *dst=edge;
  else if (srcpv[0]&0xff000000) *dst=edge;
  else *dst=0;
  dst++; src++; srcpv++;
  
  int xi=w-2;
  for (;xi-->0;dst++,src++,srcpv++) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else if (src[1]&0xff000000) *dst=edge;
    else if (srcpv[0]&0xff000000) *dst=edge;
    else *dst=0;
  }
  
  if (w>1) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else if (srcpv[0]&0xff000000) *dst=edge;
    else *dst=0;
  }
}
 
static void rb_image_trace_edges_1n(uint32_t *dst,const uint32_t *src,int w,uint32_t main,uint32_t edge) {
  const uint32_t *srcnx=src+w;
  
  if (src[0]&0xff000000) *dst=main;
  else if ((w>1)&&(src[1]&0xff000000)) *dst=edge;
  else if (srcnx[0]&0xff000000) *dst=edge;
  else *dst=0;
  dst++; src++; srcnx++;
  
  int xi=w-2;
  for (;xi-->0;dst++,src++,srcnx++) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else if (src[1]&0xff000000) *dst=edge;
    else if (srcnx[0]&0xff000000) *dst=edge;
    else *dst=0;
  }
  
  if (w>1) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else if (srcnx[0]&0xff000000) *dst=edge;
    else *dst=0;
  }
}
 
static void rb_image_trace_edges_1(uint32_t *dst,const uint32_t *src,int w,uint32_t main,uint32_t edge) {
  
  if (src[0]&0xff000000) *dst=main;
  else if ((w>1)&&(src[1]&0xff000000)) *dst=edge;
  else *dst=0;
  dst++; src++;
  
  int xi=w-2;
  for (;xi-->0;dst++,src++) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else if (src[1]&0xff000000) *dst=edge;
    else *dst=0;
  }
  
  if (w>1) {
    if (src[0]&0xff000000) *dst=main;
    else if (src[-1]&0xff000000) *dst=edge;
    else *dst=0;
  }
}
 
void rb_image_trace_edges(struct rb_image *dst,struct rb_image *src,uint32_t main,uint32_t edge) {

  int inplace=0;
  if (dst) {
    if ((dst->w!=src->w)||(dst->h!=src->h)) return;
  } else {
    inplace=1;
    if (!(dst=rb_image_new(src->w,src->h))) return;
  }
  
  uint32_t *dstrow=dst->pixels;
  const uint32_t *srcrow=src->pixels;
  int y=0;
  for (;y<src->h;y++,dstrow+=dst->w,srcrow+=src->w) {
    if (y) {
      if (y<src->h-1) {
        rb_image_trace_edges_1pn(dstrow,srcrow,src->w,main,edge);
      } else {
        rb_image_trace_edges_1p(dstrow,srcrow,src->w,main,edge);
      }
    } else if (src->h>1) {
      rb_image_trace_edges_1n(dstrow,srcrow,src->w,main,edge);
    } else {
      rb_image_trace_edges_1(dstrow,srcrow,src->w,main,edge);
    }
  }
  
  if (inplace) {
    memcpy(src->pixels,dst->pixels,src->w*src->h*4);
    rb_image_del(dst);
  }
}

/* Replace each pixel according to its alpha.
 */
 
void rb_image_replace_by_alpha(struct rb_image *image,uint32_t opaque,uint32_t transparent) {
  uint32_t *p=image->pixels;
  int i=image->w*image->h;
  switch (image->alphamode) {
    case RB_ALPHAMODE_BLEND:
    case RB_ALPHAMODE_DISCRETE: {
        for (;i-->0;p++) {
           if ((*p)&0x80000000) *p=opaque;
           else *p=transparent;
         }
      } break;
    case RB_ALPHAMODE_COLORKEY: {
        for (;i-->0;p++) {
           if (*p) *p=opaque;
           else *p=transparent;
         }
      } break;
    case RB_ALPHAMODE_OPAQUE: {
        for (;i-->0;p++) {
           *p=opaque;
         }
      } break;
  }
}

/* Darken one pixel.
 */
 
static inline uint32_t rb_pixel_darken(uint32_t src,uint8_t brightness) {
  uint8_t *v=(uint8_t*)&src;
  #if BYTE_ORDER==BIG_ENDIAN
    v[1]=(v[1]*brightness)>>8;
    v[2]=(v[2]*brightness)>>8;
    v[3]=(v[3]*brightness)>>8;
  #else
    v[0]=(v[0]*brightness)>>8;
    v[1]=(v[1]*brightness)>>8;
    v[2]=(v[2]*brightness)>>8;
  #endif
  return src;
}

/* Darken image.
 */
 
void rb_image_darken(struct rb_image *image,uint8_t brightness) {
  if (brightness>=0xff) return;
  uint32_t *dst=image->pixels;
  int i=image->w*image->h;
  
  if (!brightness) {
    if (image->alphamode==RB_ALPHAMODE_OPAQUE) {
      memset(dst,0,i<<2);
    } else if (image->alphamode==RB_ALPHAMODE_COLORKEY) {
      for (;i-->0;dst++) {
        if (*dst) *dst=0x01000000;
      }
    } else { // DISCRETE,BLEND
      for (;i-->0;dst++) {
        *dst=(*dst)&0xff000000;
      }
    }
  } else if (image->alphamode==RB_ALPHAMODE_COLORKEY) {
    for (;i-->0;dst++) {
      if (!*dst) continue;
      *dst=rb_pixel_darken(*dst,brightness);
      if (!*dst) *dst=0x01000000;
    }
  } else {
    for (;i-->0;dst++) {
      *dst=rb_pixel_darken(*dst,brightness);
    }
  }
}
