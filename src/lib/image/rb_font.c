#include "rabbit/rb_internal.h"
#include "rabbit/rb_image.h"
#include "rabbit/rb_font.h"
#include <stdarg.h>

/* Minimal 3x5 font.
 */

/*
 !"#$%&'()*+,-./
0123456789:;<=>?
@ABCDEFGHIJKLMNO
PQRSTUVWXYZ[\]^_
`abcdefghijklmno
pqrstuvwxyz{|}~
*/
static const uint16_t rb_minimal_glyphbits[96]={
  000000,022202,055000,057575,076737,041241,024257,022000,012221,042224,052725,002720,000062,000700,000002,001240, // 0x20..0x2f
  025552,062227,061247,061216,055711,074616,034652,071111,025252,035311,002020,002062,012421,007070,042124,061202, // 0x30..0x3f
  025543,025755,065656,034443,065556,074647,074644,034553,055755,072227,011152,055655,044447,077755,017774,025552, // 0x40..0x4f
  065644,025553,065655,034216,072222,055557,055564,055777,055255,055222,071247,032223,004210,062226,025000,000007, // 0x50..0x5f
  042100,025755,065656,034443,065556,074647,074644,034553,055755,072227,011152,055655,044447,077755,017774,025552, // 0x60..0x6f
  065644,025553,065655,034216,072222,055557,055564,055777,055255,055222,071247,032223,032423,022222,062126,000000, // 0x70..0x7f
};

static void rb_font_unpack_minimal_glyph(uint32_t *dst,int stride,uint16_t src) {
  int yi=5; for (;yi-->0;dst+=stride,src<<=3) {
    dst[0]=(src&0x4000)?0xff000000:0x00000000;
    dst[1]=(src&0x2000)?0xff000000:0x00000000;
    dst[2]=(src&0x1000)?0xff000000:0x00000000;
  }
}
 
struct rb_image *rb_font_generate_minimal() {
  struct rb_image *image=rb_image_new(4*16,6*6);
  if (!image) return 0;
  image->alphamode=RB_ALPHAMODE_COLORKEY;
  
  int longstride=image->w*6;
  uint32_t *dstrow=image->pixels;
  const uint16_t *srcbits=rb_minimal_glyphbits;
  int yi=6; for (;yi-->0;dstrow+=longstride) {
    uint32_t *dstp=dstrow;
    int xi=16; for (;xi-->0;dstp+=4,srcbits++) {
      rb_font_unpack_minimal_glyph(dstp,image->w,*srcbits);
    }
  }
  
  return image;
}

/* Draw one glyph.
 */
 
static void rb_font_draw_glyph(
  uint32_t *dst,int dststride,
  const uint32_t *src,int srcstride,
  int w,int h
) {
  w<<=2;
  for (;h-->0;dst+=dststride,src+=srcstride) {
    memcpy(dst,src,w);
  }
}

/* Print text from a font image, main entry point.
 */
 
struct rb_image *rb_font_print(
  const struct rb_image *font,
  int fontcontent,
  int flags,
  int wlimit,
  const char *src,int srcc
) {
  if (!font) return 0;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }

  if (flags&RB_FONT_FLAG_UTF8) {
    fprintf(stderr,"%s: TODO: Decode UTF-8\n",__func__);//TODO
    return 0;
  }
  if (wlimit>0) {
    fprintf(stderr,"%s: TODO: Break lines\n",__func__);//TODO
    return 0;
  }
  
  // Ensure (font) agrees with (fontcontent) and measure glyphs.
  int colw,rowh,ch0,rowc;
  switch (fontcontent) {
    case RB_FONTCONTENT_BYTE: {
        if (font->w%16) return 0;
        if (font->h%16) return 0;
        colw=font->w/16;
        rowh=font->h/16;
        rowc=16;
        ch0=0;
      } break;
    case RB_FONTCONTENT_ASCII: {
        if (font->w%16) return 0;
        if (font->h%8) return 0;
        colw=font->w/16;
        rowh=font->h/8;
        rowc=8;
        ch0=0;
      } break;
    case RB_FONTCONTENT_G0: {
        if (font->w%16) return 0;
        if (font->h%6) return 0;
        colw=font->w/16;
        rowh=font->h/6;
        rowc=6;
        ch0=0x20;
      } break;
    default: return 0;
  }
  if (!colw||!rowh) return 0;
  
  // Determine starting point and total size, based on input length, glyph size, and margins.
  //TODO Auto-breaking lines will be a big change here, same with UTF-8.
  int x0=0,y0=0;
  int w=srcc*colw;
  int h=rowh;
  if (flags&RB_FONT_FLAG_MARGINL) { x0=1; w++; }
  if (flags&RB_FONT_FLAG_MARGINR) w++;
  if (flags&RB_FONT_FLAG_MARGINT) { y0=1; h++; }
  if (flags&RB_FONT_FLAG_MARGINB) h++;
  
  struct rb_image *image=rb_image_new(w,h);
  if (!image) return 0;
  image->alphamode=RB_ALPHAMODE_COLORKEY;
  uint32_t *dst=image->pixels+y0*image->w+x0;
  for (;srcc-->0;src++,dst+=colw) {
    unsigned char ch=*src;
    if (ch<ch0) continue;
    ch-=ch0;
    int row=ch>>4;
    if (row>=rowc) continue;
    int col=ch&15;
    rb_font_draw_glyph(dst,image->w,font->pixels+(row*rowh)*font->w+(col*colw),font->w,colw,rowh);
  }
  
  return image;
}

/* Print text with format string.
 */
 
struct rb_image *rb_font_printf(
  const struct rb_image *font,
  int fontcontent,
  int flags,
  int wlimit,
  const char *fmt,...
) {
  va_list vargs;
  va_start(vargs,fmt);
  char tmp[256];
  int tmpc=vsnprintf(tmp,sizeof(tmp),fmt,vargs);
  if ((tmpc>=0)&&(tmpc<sizeof(tmp))) {
    return rb_font_print(font,fontcontent,flags,wlimit,tmp,tmpc);
  }
  char *v=malloc(tmpc+1);
  if (!v) return 0;
  va_start(vargs,fmt);
  int vc=vsnprintf(v,tmpc+1,fmt,vargs);
  if ((vc<0)||(vc>tmpc)) {
    free(v);
    return 0;
  }
  struct rb_image *image=rb_font_print(font,fontcontent,flags,wlimit,v,vc);
  free(v);
  return image;
}
