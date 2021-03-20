#include "rb_cli.h"
#include "rb_png.h"
#include "rabbit/rb_fs.h"
#include "rabbit/rb_image.h"

/* Read/write pixel primitives.
 */
 
static uint32_t rb_imagec_rdpx_y(const uint8_t *src) {
  // we treat grayscale input as alpha-only
  return (*src)<<24;
}

static uint32_t rb_imagec_rdpx_rgb(const uint8_t *src) {
  return 0xff000000|(src[0]<<16)|(src[1]<<8)|src[2];
}

static uint32_t rb_imagec_rdpx_ya(const uint8_t *src) {
  return (src[1]<<24)|(src[0]<<16)|(src[0]<<8)|src[0];
}

static uint32_t rb_imagec_rdpx_rgba(const uint8_t *src) {
  return (src[3]<<24)|(src[0]<<16)|(src[1]<<8)|src[2];
}

static void rb_imagec_wrpx_rgba(uint8_t *dst,uint32_t argb) {
  dst[0]=argb>>24;
  dst[1]=argb>>16;
  dst[2]=argb>>8;
  dst[3]=argb;
}

static void rb_imagec_wrpx_rgb(uint8_t *dst,uint32_t argb) {
  dst[0]=argb>>16;
  dst[1]=argb>>8;
  dst[2]=argb;
}

static void rb_imagec_wrpx_rgbck(uint8_t *dst,uint32_t argb) {
  if (argb&0xff000000) {
    dst[0]=argb>>16;
    dst[1]=argb>>8;
    dst[2]=argb;
    if (!dst[0]&&!dst[1]&&!dst[2]) dst[2]=0x01; // 000001 should be indistinguishable from black
  } else { 
    dst[0]=dst[1]=dst[2]=0;
  }
}

static void rb_imagec_wrpx_a8(uint8_t *dst,uint32_t argb) {
  *dst=argb>>24;
}

/* Select format.
 */
 
static int rb_imagec_select_format_from_png(const struct rb_png_image *png) {
  switch (png->colortype) {
  
    case 0: {
        // Gray no-alpha inputs, we turn the luminance into alpha, eg for fonts.
        // If the whole image consists of 0x00 and 0xff, use A1, otherwise A8.
        const uint8_t *row=png->pixels;
        int yi=png->h;
        for (;yi-->0;row+=png->stride) {
          const uint8_t *p=row;
          int xi=png->w;
          for (;xi-->0;p++) {
            if ((*p>0x00)&&(*p<0xff)) return RB_IMAGE_FORMAT_A8;
          }
        }
        return RB_IMAGE_FORMAT_A1;
      }
      
    case 2: {
        // RGB opaque in, opaque out, easy.
        return RB_IMAGE_FORMAT_RGB;
      }
      
    case 4: {
        // If the alphas are all 0xff, ignore them and treat like colortype 0.
        // Otherwise, use RGBA.
        int have_midtones=0;
        const uint8_t *row=png->pixels;
        int yi=png->h;
        for (;yi-->0;row+=png->stride) {
          const uint8_t *p=row;
          int xi=png->w;
          for (;xi-->0;p+=2) {
            if (p[1]!=0xff) return RB_IMAGE_FORMAT_RGBA;
            if ((p[0]>0x00)&&(p[0]<0xff)) have_midtones=1;
          }
        }
        if (have_midtones) return RB_IMAGE_FORMAT_A8;
        return RB_IMAGE_FORMAT_A1;
      }
      
    case 6: {
        // If there's at least one alpha other than 0x00 and 0xff, keep RGBA.
        // All 0xff, use opaque RGB.
        // 0x00 and 0xff, use RGBCK.
        int have_zero=0,have_one=0;
        const uint8_t *row=png->pixels;
        row+=3;
        int yi=png->h;
        for (;yi-->0;row+=png->stride) {
          const uint8_t *p=row;
          int xi=png->w;
          for (;xi-->0;p+=4) {
            if (*p==0x00) have_zero=1;
            else if (*p==0xff) have_one=1;
            else return RB_IMAGE_FORMAT_RGBA;
          }
        }
        if (have_zero) return RB_IMAGE_FORMAT_RGBCK;
        return RB_IMAGE_FORMAT_RGB;
      }
      
  }
  return -1;
}

/* Convert in memory.
 */
 
static int rb_imagec(void *dstpp,const char *src,int srcc,struct rb_cli *cli) {

  struct rb_png_image png={0};
  if (rb_png_decode(&png,src,srcc)<0) {
    fprintf(stderr,"%s: Failed to decode PNG: %.*s\n",cli->datapath,png.messagec,png.message);
    rb_png_image_cleanup(&png);
    return -1;
  }
  
  if ((png.w>0x1000)||(png.h>0x1000)) {
    fprintf(stderr,"%s: Image size %dx%d exceeds limit of %d\n",cli->datapath,png.w,png.h,0x1000);
    rb_png_image_cleanup(&png);
    return -1;
  }
  
  uint32_t (*rdpx)(const uint8_t *src)=0;
  int srcxstride=0;
  switch (png.colortype) {
    case 0: srcxstride=1; rdpx=rb_imagec_rdpx_y; break;
    case 2: srcxstride=3; rdpx=rb_imagec_rdpx_rgb; break;
    case 4: srcxstride=2; rdpx=rb_imagec_rdpx_ya; break;
    case 6: srcxstride=4; rdpx=rb_imagec_rdpx_rgba; break;
    default: {
        rb_png_image_cleanup(&png);
        return -1;
      }
  }
  
  void (*wrpx)(uint8_t *dst,uint32_t argb)=0;
  int format=rb_imagec_select_format_from_png(&png);
  int dststride,dstxstride;
  switch (format) {
    case RB_IMAGE_FORMAT_RGBA: {
        dststride=png.w<<2;
        dstxstride=4;
        wrpx=rb_imagec_wrpx_rgba;
      } break;
    case RB_IMAGE_FORMAT_RGB: {
        dststride=png.w*3;
        dstxstride=3;
        wrpx=rb_imagec_wrpx_rgb;
      } break;
    case RB_IMAGE_FORMAT_RGBCK: {
        dststride=png.w*3;
        dstxstride=3;
        wrpx=rb_imagec_wrpx_rgbck;
      } break;
    case RB_IMAGE_FORMAT_A8: {
        dststride=png.w;
        dstxstride=1;
        wrpx=rb_imagec_wrpx_a8;
      } break;
    case RB_IMAGE_FORMAT_A1: {
        dststride=(png.w+7)>>3;
        dstxstride=0;
      } break;
    default: {
        fprintf(stderr,"%s: No valid output formats for depth=%d colortype=%d\n",cli->datapath,png.depth,png.colortype);
        rb_png_image_cleanup(&png);
        return -1;
      }
  }
  
  int dstc=4+dststride*png.h;
  uint8_t *dst=malloc(dstc);
  if (!dst) {
    rb_png_image_cleanup(&png);
    return -1;
  }
  
  dst[0]=format;
  dst[1]=(png.w-1)>>4;
  dst[2]=((png.w-1)<<4)|((png.h-1)>>8);
  dst[3]=png.h-1;
  
  uint8_t *dstrow=dst+4;
  const uint8_t *srcrow=png.pixels;
  int i=png.h;
  if (dstxstride) { // output pixels at least one byte
    for (;i-->0;dstrow+=dststride,srcrow+=png.stride) {
      uint8_t *dstp=dstrow;
      const uint8_t *srcp=srcrow;
      int xi=png.w; for (;xi-->0;dstp+=dstxstride,srcp+=srcxstride) {
        wrpx(dstp,rdpx(srcp));
      }
    }
  } else { // output pixels 1 bit
    for (;i-->0;dstrow+=dststride,srcrow+=png.stride) {
      uint8_t *dstp=dstrow;
      const uint8_t *srcp=srcrow;
      uint8_t mask=0x80;
      int xi=png.w; for (;xi-->0;srcp+=srcxstride) {
        if (rdpx(srcp)) (*dstp)|=mask;
        if (!(mask>>=1)) {
          dstp++;
          mask=0x80;
        }
      }
    }
  }
  
  rb_png_image_cleanup(&png);
  *(void**)dstpp=dst;
  return dstc;
}

/* Main entry point.
 */
 
int rb_cli_main_imagec(struct rb_cli *cli) {
  if (!cli->dstpath||!cli->dstpath[0]) {
    fprintf(stderr,"%s: '--dst=PATH' required\n",cli->exename);
    return -1;
  }
  
  void *src=0;
  int srcc=rb_file_read(&src,cli->datapath);
  if (srcc<0) {
    fprintf(stderr,"%s:ERROR: Failed to read file\n",cli->datapath);
    return -1;
  }
  
  void *dst=0;
  int dstc=rb_imagec(&dst,src,srcc,cli);
  free(src);
  if (dstc<0) {
    fprintf(stderr,"%s:ERROR: Failed to convert image\n",cli->datapath);
    return -1;
  }
  
  if (rb_file_write(cli->dstpath,dst,dstc)<0) {
    fprintf(stderr,"%s:ERROR: Failed to write output (%d bytes)\n",cli->dstpath,dstc);
    free(dst);
    return -1;
  }
  free(dst);
  return 0;
}
