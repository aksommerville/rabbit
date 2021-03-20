#include "rb_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define RB_PNG_CHUNK_IHDR (('I'<<24)|('H'<<16)|('D'<<8)|'R')
#define RB_PNG_CHUNK_IDAT (('I'<<24)|('D'<<16)|('A'<<8)|'T')
#define RB_PNG_CHUNK_IEND (('I'<<24)|('E'<<16)|('N'<<8)|'D')

/* Decoder context.
 */
 
struct rb_png_decoder {
  struct rb_png_image *image;
  z_stream z;
  int zinit;
  const uint8_t *src;
  int srcc;
  int rowbuflen; // stride+1
  uint8_t *rowbuf; // use as IHDR-present flag
  int xstride; // horz step in bytes (from pixelsize, for filtering)
  int y;
};

static void rb_png_decoder_cleanup(struct rb_png_decoder *decoder) {
  if (decoder->zinit) inflateEnd(&decoder->z);
  if (decoder->rowbuf) free(decoder->rowbuf);
}

/* Unfilter one row.
 */
 
static inline uint8_t rb_paeth(uint8_t a,uint8_t b,uint8_t c) {
  int p=a+b-c;
  int pa=p-a; if (pa<0) pa=-pa;
  int pb=p-b; if (pb<0) pb=-pb;
  int pc=p-c; if (pc<0) pc=-pc;
  if ((pa<=pb)&&(pa<=pc)) return a;
  if (pb<=pc) return b;
  return c;
}
 
static int rb_png_unfilter_row(
  uint8_t *dst,
  uint8_t filter,
  const uint8_t *src,
  const uint8_t *prv,
  struct rb_png_decoder *decoder
) {
  switch (filter) {
  
    case 0x00: {
        memcpy(dst,src,decoder->image->stride);
      } return 0;
    
    case 0x01: {
        memcpy(dst,src,decoder->xstride);
        int i=decoder->xstride;
        for (;i<decoder->image->stride;i++) {
          dst[i]=src[i]+dst[i-decoder->xstride];
        }
      } return 0;
      
    case 0x02: if (prv) {
        int i=0;
        for (;i<decoder->image->stride;i++) {
          dst[i]=src[i]+prv[i];
        }
      } else {
        memcpy(dst,src,decoder->image->stride);
      } return 0;
      
    case 0x03: if (prv) {
        int i=0;
        for (;i<decoder->xstride;i++) {
          dst[i]=src[i]+(prv[i]>>1);
        }
        for (;i<decoder->image->stride;i++) {
          dst[i]=src[i]+((prv[i]+dst[i-decoder->xstride])>>1);
        }
      } else {
        memcpy(dst,src,decoder->xstride);
        int i=decoder->xstride;
        for (;i<decoder->image->stride;i++) {
          dst[i]=src[i]+(dst[i-decoder->xstride]>>1);
        }
      } return 0;
      
    case 0x04: if (prv) {
        int i=0;
        for (;i<decoder->xstride;i++) {
          dst[i]=src[i]+rb_paeth(0,prv[i],0);
        }
        for (;i<decoder->image->stride;i++) {
          dst[i]=src[i]+rb_paeth(dst[i-decoder->xstride],prv[i],prv[i-decoder->xstride]);
        }
      } else {
        int i=0;
        for (;i<decoder->xstride;i++) {
          dst[i]=src[i];
        }
        for (;i<decoder->image->stride;i++) {
          dst[i]=src[i]+dst[i-decoder->xstride];
        }
      } return 0;
  
  }
  return rb_png_fail(decoder->image,
    "Illegal filter 0x%02x at row %d/%d",
    filter,decoder->y,decoder->image->h
  );
}

/* Row buffer is populated. Unfilter and add it to the image.
 * This queues up the next row (updates z->(next_out,avail_out), and (y)).
 */
 
static int rb_png_receive_row(struct rb_png_decoder *decoder) {
  if (decoder->y<decoder->image->h) {
    uint8_t filter=decoder->rowbuf[0];
    const uint8_t *src=decoder->rowbuf+1;
    uint8_t *dst=((uint8_t*)decoder->image->pixels)+decoder->y*decoder->image->stride;
    const uint8_t *prv=0;
    if (decoder->y) prv=dst-decoder->image->stride;
    if (rb_png_unfilter_row(dst,filter,src,prv,decoder)<0) return -1;
    decoder->y++;
  }
  decoder->z.next_out=(Bytef*)decoder->rowbuf;
  decoder->z.avail_out=decoder->rowbuflen;
  return 0;
}

/* Receive IDAT.
 */
 
static int rb_png_decode_IDAT(struct rb_png_decoder *decoder,const uint8_t *src,int srcc) {
  if (!decoder->rowbuf) return rb_png_fail(decoder->image,"IDAT before IHDR");
  decoder->z.next_in=(Bytef*)src;
  decoder->z.avail_in=srcc;
  while (decoder->z.avail_in) {
    if (!decoder->z.avail_out) {
      if (rb_png_receive_row(decoder)<0) return -1;
    }
    int err=inflate(&decoder->z,Z_NO_FLUSH);
    if (err<0) {
      err=inflate(&decoder->z,Z_SYNC_FLUSH);
      if (err<0) return rb_png_fail(decoder->image,"inflate(): %d",err);
    }
  }
  return 0;
}

/* Receive IHDR.
 */
 
static int rb_png_decode_IHDR(struct rb_png_decoder *decoder,const uint8_t *src,int srcc) {
  if (decoder->rowbuf) return rb_png_fail(decoder->image,"Multiple IHDR");
  if (srcc<13) return rb_png_fail(decoder->image,"Invalid IHDR length %d",srcc);
  
  decoder->image->w=(src[0]<<24)|(src[1]<<16)|(src[2]<<8)|src[3];
  decoder->image->h=(src[4]<<24)|(src[5]<<16)|(src[6]<<8)|src[7];
  decoder->image->depth=src[8];
  decoder->image->colortype=src[9];
  decoder->image->compression=src[10];
  decoder->image->filter=src[11];
  decoder->image->interlace=src[12];
  
  if (decoder->image->compression||decoder->image->filter||(decoder->image->interlace>1)) {
    // actually illegal
    return rb_png_fail(decoder->image,
      "Unsupported compression, filter, or interlace (%d,%d,%d)",
      decoder->image->compression,decoder->image->filter,decoder->image->interlace
    );
  }
  // We violate the spec by forbidding Adam7 interlaced images. Because they're complicated and pointless.
  if (decoder->image->interlace) return rb_png_fail(decoder->image,
    "Interlaced PNG not supported"
  );
  
  // Zero size is illegal, and we enforce an out-of-spec upper limit.
  if (
    (decoder->image->w<1)||(decoder->image->w>RB_PNG_SIZE_LIMIT)||
    (decoder->image->h<1)||(decoder->image->h>RB_PNG_SIZE_LIMIT)
  ) return rb_png_fail(decoder->image,
    "Unsupported dimensions %dx%d",
    decoder->image->w,decoder->image->h
  );
  
  // We support only a limited set of pixel formats.
  // As a courtesy to the user, distinguish whether we are rejecting a valid format, or an illegal one.
  #define ILLEGAL return rb_png_fail(decoder->image, \
    "Invalid depth and colortype (%d,%d)", \
    decoder->image->depth,decoder->image->colortype \
  );
  #define NOTSUP return rb_png_fail(decoder->image, \
    "Depth and colortype (%d,%d) is legal but not supported by this decoder. Use 8-bit gray or RGB.", \
    decoder->image->depth,decoder->image->colortype \
  );
  int chanc;
  switch (decoder->image->colortype) {
    case 0: switch (decoder->image->depth) {
        case 1: NOTSUP
        case 2: NOTSUP
        case 4: NOTSUP
        case 8: chanc=1; break;
        case 16: NOTSUP
        default: ILLEGAL
      } break;
    case 2: switch (decoder->image->depth) {
        case 8: chanc=3; break;
        case 16: NOTSUP
        default: ILLEGAL
      } break;
    case 3: switch (decoder->image->depth) {
        case 1: NOTSUP
        case 2: NOTSUP
        case 4: NOTSUP
        case 8: NOTSUP
        default: ILLEGAL
      } break;
    case 4: switch (decoder->image->depth) {
        case 8: chanc=2; break;
        case 16: NOTSUP
        default: ILLEGAL
      } break;
    case 6: switch (decoder->image->depth) {
        case 8: chanc=4; break;
        case 16: NOTSUP
        default: ILLEGAL
      } break;
    default: ILLEGAL
  }
  #undef ILLEGAL
  #undef NOTSUP
  
  // OK header checks out, start allocating things.
  int pixelsize=decoder->image->depth*chanc;
  decoder->xstride=(pixelsize+7)>>3;
  decoder->image->stride=(pixelsize*decoder->image->w+7)>>3; // +7 is redundant since we only allow full bytes, whatever
  if (!(decoder->image->pixels=calloc(decoder->image->stride,decoder->image->h))) return -1;
  decoder->rowbuflen=1+decoder->image->stride;
  if (!(decoder->rowbuf=malloc(decoder->rowbuflen))) return -1;
  if (inflateInit(&decoder->z)<0) return rb_png_fail(decoder->image,"inflateInit failed");
  decoder->zinit=1;
  decoder->z.next_out=(Bytef*)decoder->rowbuf;
  decoder->z.avail_out=decoder->rowbuflen;
  
  return 0;
}

/* Finish decoding, assert completion.
 */
 
static int rb_png_decode_finish(struct rb_png_decoder *decoder) {
  if (!decoder->rowbuf) return rb_png_fail(decoder->image,"No IHDR");
  
  // Flush decompressor.
  int zend=0;
  while (decoder->y<decoder->image->h) {
    if (!decoder->z.avail_out) {
      if (rb_png_receive_row(decoder)<0) return -1;
    }
    if (zend) break;
    int err=inflate(&decoder->z,Z_FINISH);
    if (err<0) {
      return rb_png_fail(decoder->image,"inflate(): %d",err);
    }
    if (err==Z_STREAM_END) zend=1;
  }
  
  // Assert we got all the pixels.
  if (decoder->y<decoder->image->h) return rb_png_fail(decoder->image,
    "Incomplete image (%d of %d rows)",
    decoder->y,decoder->image->h
  );
  
  return 0;
}

/* Decode in context.
 * Split into chunks and dispatch.
 */
 
static int rb_png_decode_inner(struct rb_png_decoder *decoder) {
  if ((decoder->srcc<8)||memcmp(decoder->src,"\x89PNG\r\n\x1a\n",8)) {
    return rb_png_fail(decoder->image,"Signature mismatch");
  }
  int srcp=8;
  int stopp=decoder->srcc-8;
  while (srcp<=stopp) {
    int p0=srcp;
    const uint8_t *in=decoder->src+p0;
    int chunklen=(in[0]<<24)|(in[1]<<16)|(in[2]<<8)|in[3];
    int chunktype=(in[4]<<24)|(in[5]<<16)|(in[6]<<8)|in[7];
    srcp+=8;
    in+=8;
    if ((chunklen<0)||(srcp>decoder->srcc-4-chunklen)) {
      return rb_png_fail(decoder->image,
        "Invalid chunk length %d at %d/%d",
        chunklen,p0,decoder->srcc
      );
    }
    srcp+=chunklen+4;
    switch (chunktype) {
      case RB_PNG_CHUNK_IHDR: if (rb_png_decode_IHDR(decoder,in,chunklen)<0) return -1; break;
      case RB_PNG_CHUNK_IDAT: if (rb_png_decode_IDAT(decoder,in,chunklen)<0) return -1; break;
      case RB_PNG_CHUNK_IEND: srcp=stopp+1; break;
    }
  }
  return rb_png_decode_finish(decoder);
}

/* Decode, main entry point.
 */

int rb_png_decode(
  struct rb_png_image *image,
  const void *src,int srcc
) {
  rb_png_image_cleanup(image);
  struct rb_png_decoder decoder={
    .image=image,
    .src=src,
    .srcc=srcc,
  };
  if (rb_png_decode_inner(&decoder)<0) {
    rb_png_decoder_cleanup(&decoder);
    return rb_png_fail(image,"Failed to decode, no detail");
  }
  rb_png_decoder_cleanup(&decoder);
  return 0;
}
