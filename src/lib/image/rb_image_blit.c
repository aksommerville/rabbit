#include "rabbit/rb_internal.h"
#include "rabbit/rb_image.h"

/* Clear image.
 */
 
int rb_image_clear(struct rb_image *image,uint32_t argb) {
  if (!image) return -1;
  if (argb) {
    uint32_t *dst=image->pixels;
    int i=image->w*image->h;
    for (;i-->0;dst++) *dst=argb;
  } else {
    memset(image->pixels,0,image->w*image->h*4);
  }
  return 0;
}

/* Blit to image with adjustment.
 */
 
int rb_image_blit_safe(
  struct rb_image *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
) {
  if (rb_image_check_bounds(dst,&dstx,&dsty,src,&srcx,&srcy,&w,&h,xform)>0) {
    rb_image_blit_unchecked(dst,dstx,dsty,src,srcx,srcy,w,h,xform,blend,userdata);
  }
  return 0;
}

/* Blit to image.
 */
 
void rb_image_blit_unchecked(
  struct rb_image *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
) {

  // No alpha or xform? Use rowwise memcpy.
  if (!xform&&!blend&&(src->alphamode==RB_ALPHAMODE_OPAQUE)) {
    int cpc=w<<2;
    uint32_t *dstrow=dst->pixels+dsty*dst->w+dstx;
    const uint32_t *srcrow=src->pixels+srcy*src->w+srcx;
    for (;h-->0;dstrow+=dst->w,srcrow+=src->w) {
      memcpy(dstrow,srcrow,cpc);
    }
    return;
  }

  /* Iterate LRTB in (dst).
   * So dst delta minor is always one, and major is basically stride minus width.
   * (ddstmajor) changes based on RB_XFORM_SWAP, since that changes the meaning of (w,h).
   * (xform) is expressed mostly in the (src) iterator.
   */
  uint32_t *dstp=dst->pixels+dsty*dst->w+dstx;
  int dstminorc,dstmajorc;
  if (xform&RB_XFORM_SWAP) {
    dstminorc=h;
    dstmajorc=w;
  } else {
    dstminorc=w;
    dstmajorc=h;
  }
  int ddstmajor=dst->w-dstminorc;
  
  const uint32_t *srcp=src->pixels+srcy*src->w+srcx;
  int dsrcminor,dsrcmajor;
  switch (xform) {
    case RB_XFORM_NONE: {
        dsrcminor=1;
        dsrcmajor=src->w-w;
      } break;
    case RB_XFORM_XREV: {
        srcp+=w-1;
        dsrcminor=-1;
        dsrcmajor=src->w+w;
      } break;
    case RB_XFORM_YREV: {
        srcp+=src->w*(h-1);
        dsrcminor=1;
        dsrcmajor=-src->w-w;
      } break;
    case RB_XFORM_XREV|RB_XFORM_YREV: {
        srcp+=src->w*(h-1)+w-1;
        dsrcminor=-1;
        dsrcmajor=w-src->w;
      } break;
    case RB_XFORM_SWAP: {
        dsrcminor=src->w;
        dsrcmajor=1-(src->w*h);
      } break;
    case RB_XFORM_SWAP|RB_XFORM_XREV: {
        srcp+=w-1;
        dsrcminor=src->w;
        dsrcmajor=-(src->w*h)-1;
      } break;
    case RB_XFORM_SWAP|RB_XFORM_YREV: {
        srcp+=src->w*(h-1);
        dsrcminor=-src->w;
        dsrcmajor=src->w*h+1;
      } break;
    case RB_XFORM_SWAP|RB_XFORM_XREV|RB_XFORM_YREV: {
        srcp+=src->w*(h-1)+w-1;
        dsrcminor=-src->w;
        dsrcmajor=src->w*h-1;
      } break;
    default: return; // invalid xform
  }
  
  #define ITERATE(ppcode) { \
    int i=dstmajorc; \
    while (i-->0) { \
      int j=dstminorc; \
      while (j-->0) { \
        ppcode \
        srcp+=dsrcminor; \
        dstp+=1; \
      } \
      srcp+=dsrcmajor; \
      dstp+=ddstmajor; \
    } \
  }
  
  /* Select a per-pixel strategy based on (blend) or (src->alphamode).
   */
  switch (src->alphamode) {
  
    case RB_ALPHAMODE_BLEND: if (blend) ITERATE({
        *dstp=blend(*dstp,*srcp,userdata);
      }) else ITERATE({
        uint8_t a=(*srcp)>>24;
        if (a==0x00) ;
        else if (a==0xff) {
          *dstp=*srcp;
        } else {
          uint8_t dsta=0xff-a;
          uint8_t r=((((*dstp)>>16)&0xff)*dsta+(((*srcp)>>16)&0xff)*a)>>8;
          uint8_t g=((((*dstp)>>8)&0xff)*dsta+(((*srcp)>>8)&0xff)*a)>>8;
          uint8_t b=(((*dstp)&0xff)*dsta+((*srcp)&0xff)*a)>>8;
          *dstp=(r<<16)|(g<<8)|b;
        }
      }) break;
    
    case RB_ALPHAMODE_COLORKEY: if (blend) ITERATE({
        if (*srcp) *dstp=blend(*dstp,*srcp,userdata);
      }) else ITERATE({
        if (*srcp) *dstp=*srcp;
      }) break;
    
    case RB_ALPHAMODE_DISCRETE: if (blend) ITERATE({
        if (*srcp>=0x80000000) *dstp=blend(*dstp,*srcp,userdata);
      }) else ITERATE({
        if (*srcp>=0x80000000) *dstp=*srcp;
      }) break;
    
    case RB_ALPHAMODE_OPAQUE: if (blend) ITERATE({
        *dstp=blend(*dstp,*srcp,userdata);
      }) else ITERATE({
        *dstp=*srcp;
      }) break;
    
    default: return; // invalid alphamode
  }
  #undef ITERATE
  
  //...and that's all there is to it!
}

/* Blit to image: Assert bounds.
 */
 
int rb_image_blit(
  struct rb_image *dst,int dstx,int dsty,
  const struct rb_image *src,int srcx,int srcy,
  int w,int h,
  uint8_t xform,
  uint32_t (*blend)(uint32_t dst,uint32_t src,void *userdata),
  void *userdata
) {
  if (w<1) return 0;
  if (h<1) return 0;
  if (dstx<0) return -1;
  if (dsty<0) return -1;
  if (srcx<0) return -1;
  if (srcy<0) return -1;
  if (dstx>dst->w-w) return -1;
  if (dsty>dst->h-h) return -1;
  if (srcx>src->w-w) return -1;
  if (srcy>src->h-h) return -1;
  rb_image_blit_unchecked(dst,dstx,dsty,src,srcx,srcy,w,h,xform,blend,userdata);
  return 1;
}

/* Check blit bounds.
 */
 
int rb_image_check_bounds(
  const struct rb_image *dst,int *dstx,int *dsty,
  const struct rb_image *src,int *srcx,int *srcy,
  int *w,int *h,
  uint8_t xform
) {

  // In the common case of no transform, it's pretty easy.
  if (!xform) {
    if (*dstx<0) { (*srcx)-=(*dstx); (*w)+=(*dstx); (*dstx)=0; }
    if (*dsty<0) { (*srcy)-=(*dsty); (*h)+=(*dsty); (*dsty)=0; }
    if (*srcx<0) { (*dstx)-=(*srcx); (*w)+=(*srcx); (*srcx)=0; }
    if (*srcy<0) { (*dsty)-=(*srcy); (*h)+=(*srcy); (*srcy)=0; }
    if ((*dstx)>dst->w-(*w)) (*w)=dst->w-(*dstx);
    if ((*dsty)>dst->h-(*h)) (*h)=dst->h-(*dsty);
    if ((*srcx)>src->w-(*w)) (*w)=src->w-(*srcx);
    if ((*srcy)>src->h-(*h)) (*h)=src->h-(*srcy);
    if ((*w)<1) return 0;
    if ((*h)<1) return 0;
    return 1;
  }
  
  if (*dstx<0) {
    if (xform&RB_XFORM_SWAP) {
      (*h)+=*dstx;
      if (!(xform&RB_XFORM_YREV)) (*srcy)-=*dstx;
    } else {
      (*w)+=*dstx;
      if (!(xform&RB_XFORM_XREV)) (*srcx)-=*dstx;
    }
    *dstx=0;
  }
  
  if (*dsty<0) {
    if (xform&RB_XFORM_SWAP) {
      (*w)+=*dsty;
      if (!(xform&RB_XFORM_XREV)) (*srcx)-=*dsty;
    } else {
      (*h)+=*dsty;
      if (!(xform&RB_XFORM_YREV)) (*srcy)-=*dsty;
    }
    *dsty=0;
  }
  
  if (*srcx<0) {
    (*w)+=*srcx;
    if (xform&RB_XFORM_SWAP) {
      if (!(xform&RB_XFORM_XREV)) (*dsty)-=*srcx;
    } else {
      if (!(xform&RB_XFORM_XREV)) (*dstx)-=*srcx;
    }
    *srcx=0;
  }
  
  if (*srcy<0) {
    (*h)+=*srcy;
    if (xform&RB_XFORM_SWAP) {
      if (!(xform&RB_XFORM_YREV)) (*dstx)-=*srcy;
    } else {
      if (!(xform&RB_XFORM_YREV)) (*dsty)-=*srcy;
    }
    *srcy=0;
  }
  
  if (xform&RB_XFORM_SWAP) {
    if (*dstx>RB_FB_W-*h) {
      if (xform&RB_XFORM_YREV) (*srcy)+=(*dstx)+(*h)-dst->w;
      *h=dst->w-*dstx;
    }
    if (*dsty>RB_FB_H-*w) {
      if (xform&RB_XFORM_XREV) (*srcx)+=(*dsty)+(*w)-dst->h;
      *w=dst->h-*dsty;
    }
  } else {
    if (*dstx>RB_FB_W-*w) {
      if (xform&RB_XFORM_XREV) (*srcx)+=(*dstx)+(*w)-dst->w;
      *w=dst->w-*dstx;
    }
    if (*dsty>RB_FB_H-*h) {
      if (xform&RB_XFORM_YREV) (*srcy)+=(*dsty)+(*h)-dst->h;
      *h=dst->h-*dsty;
    }
  }
  
  if (*srcx>src->w-*w) {
    if (xform&RB_XFORM_SWAP) {
      if (xform&RB_XFORM_XREV) (*dsty)+=(*srcx)+(*w)-src->w;
    } else {
      if (xform&RB_XFORM_XREV) (*dstx)+=(*srcx)+(*w)-src->w;
    }
    *w=src->w-*srcx;
  }
  
  if (*srcy>src->h-*h) {
    if (xform&RB_XFORM_SWAP) {
      if (xform&RB_XFORM_YREV) (*dstx)+=(*srcy)+(*h)-src->h;
    } else {
      if (xform&RB_XFORM_YREV) (*dsty)+=(*srcy)+(*h)-src->h;
    }
    *h=src->h-*srcy;
  }
  
  if (*w<1) return 0;
  if (*h<1) return 0;
  return 1;
}

/* Scroll image content.
 */
 
int rb_image_scroll(struct rb_image *image,int dx,int dy) {
  if (!dx&&!dy) return 0;
  int srcx=0,srcy=0,dstx=dx,dsty=dy,w=image->w,h=image->h;
  if (dstx<0) { w+=dstx; srcx-=dstx; dstx=0; }
  else w=image->w-dstx;
  if (dsty<0) { h+=dsty; srcy-=dsty; dsty=0; }
  else h=image->h-dsty;
  if ((w<1)||(h<1)) return 0;
  
  /* Self-copy:
   *  - If y changes, we can memcpy rowwise. Just need to go the right direction.
   *  - Constant y, we need to transfer one pixel at a time.
   */
  if (dsty>srcy) { // rowwise memcpy, bottom to top
    int cpc=w<<2;
    uint32_t *dst=image->pixels+(dsty+h-1)*image->w+dstx;
    uint32_t *src=image->pixels+(srcy+h-1)*image->w+srcx;
    int i=h;
    for (;i-->0;dst-=image->w,src-=image->w) {
      memcpy(dst,src,cpc);
    }
  } else if (dsty<srcy) { // rowwise memcpy, top to bottom
    int cpc=w<<2;
    uint32_t *dst=image->pixels+dsty*image->w+dstx;
    uint32_t *src=image->pixels+srcy*image->w+srcx;
    int i=h;
    for (;i-->0;dst+=image->w,src+=image->w) {
      memcpy(dst,src,cpc);
    }
  } else if (dstx>srcx) { // pixelwise copy, right to left
    uint32_t *dstrow=image->pixels+dsty*image->w+dstx+w-1;
    uint32_t *srcrow=image->pixels+srcy*image->w+srcx+w-1;
    int yi=h;
    for (;yi-->0;dstrow+=image->w,srcrow+=image->w) {
      uint32_t *dstp=dstrow;
      uint32_t *srcp=srcrow;
      int xi=w;
      for (;xi-->0;dstp--,srcp--) *dstp=*srcp;
    }
  } else if (dstx<srcx) { // pixelwise copy, left to right
    uint32_t *dstrow=image->pixels+dsty*image->w+dstx;
    uint32_t *srcrow=image->pixels+srcy*image->w+srcx;
    int yi=h;
    for (;yi-->0;dstrow+=image->w,srcrow+=image->w) {
      uint32_t *dstp=dstrow;
      uint32_t *srcp=srcrow;
      int xi=w;
      for (;xi-->0;dstp++,srcp++) *dstp=*srcp;
    }
  }
  
  return 0;
}
