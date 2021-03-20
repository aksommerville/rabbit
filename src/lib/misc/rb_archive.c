#include "rabbit/rb_internal.h"
#include "rabbit/rb_archive.h"
#include "rabbit/rb_serial.h"
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>

#define RB_ARCHIVE_INBUF_LIMIT  0x00100000 /* 1 MB; Most compressed data we'll hold at one time. */
#define RB_ARCHIVE_RESBUF_LIMIT 0x00100000 /* Most uncompressed data; effectively a limit on resource size. */

/* Archive reader context.
 */
 
struct rb_archive {
  const char *path;
  int (*cb_res)(uint32_t type,int id,const void *src,int srcc,void *userdata);
  void *userdata;
  int fd; // close and set <0 at eof
  char *inbuf; // compressed content straight off the file
  int inbufp,inbufc,inbufa;
  uint32_t restype;
  int pvresid;
  int resid; // <0 if not yet determined
  int reslen; // expected body length, <0 if not yet determined
  char *resbuf; // decompressed stream
  int resbufp,resbufc,resbufa;
  z_stream z;
  int zinit;
};

/* Cleanup.
 */

static void rb_archive_cleanup(struct rb_archive *archive) {
  if (archive->fd>=0) close(archive->fd);
  if (archive->inbuf) free(archive->inbuf);
  if (archive->resbuf) free(archive->resbuf);
  if (archive->zinit) inflateEnd(&archive->z);
}

/* Open.
 */
 
static int rb_archive_open(struct rb_archive *archive) {
  if ((archive->fd=open(archive->path,O_RDONLY))<0) {
    return -1;
  }
  if (inflateInit(&archive->z)<0) {
    return -1;
  }
  archive->zinit=1;
  archive->restype=0;
  archive->pvresid=0;
  archive->resid=-1;
  archive->reslen=-1;
  return 0;
}

/* Detect completion.
 */
 
static int rb_archive_finished(struct rb_archive *archive) {
  if (archive->fd>=0) return 0; // can still read more
  if (archive->inbufc) return 0; // more remaining to decompress
  if (archive->resbufc) return 0; // more remaining to decode
  return 1;
}

/* Advance decoder state in decompressed (resbuf).
 * Triggers callback.
 */
 
static int rb_archive_decode(struct rb_archive *archive) {
  while (archive->resbufc>0) {
  
    // Read (resid) if we don't have it.
    // This could also be a type change.
    if (archive->resid<0) {
      int n,err;
      if ((err=rb_vlq_decode(&n,archive->resbuf+archive->resbufp,archive->resbufc))>0) {
        archive->resid=archive->pvresid+n;
        archive->reslen=-1;
        archive->pvresid=archive->resid;
        archive->resbufp+=err;
        archive->resbufc-=err;
      } else {
        if (archive->resbufc<4) return 0;
        const uint8_t *b=(uint8_t*)(archive->resbuf+archive->resbufp);
        uint32_t type=(b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];
        if ((type&0x80808080)!=0x80808080) {
          // I don't think this is possible, but if so it's bad data.
          return -1;
        }
        archive->restype=type&0x7f7f7f7f;
        archive->resbufp+=4;
        archive->resbufc-=4;
        archive->pvresid=0;
        continue; // still need resid
      }
    }
    
    // Read (reslen) if we don't have it.
    if (archive->reslen<0) {
      int n,err;
      if ((err=rb_vlq_decode(&n,archive->resbuf+archive->resbufp,archive->resbufc))>0) {
        archive->reslen=n;
        archive->resbufp+=err;
        archive->resbufc-=err;
      } else {
        if (archive->resbufc>=4) {
          // Invalid VLQ in length position, definitely an error.
          return -1;
        }
        return 0;
      }
    }
    
    // If we have the full body, send it to our owner, then reset.
    if (archive->resbufc<archive->reslen) {
      return 0;
    }
    int err=archive->cb_res(archive->restype,archive->resid,archive->resbuf+archive->resbufp,archive->reslen,archive->userdata);
    if (err) return err;
    archive->resbufp+=archive->reslen;
    archive->resbufc-=archive->reslen;
    archive->resid=-1;
    archive->reslen=-1;
  }
  return 0;
}

/* Read from file to inbuf.
 * Close file at eof.
 * May complete successfully without reading anything.
 */
 
static int rb_archive_fill_inbuf(struct rb_archive *archive) {
  if (archive->fd<0) return 0;
  
  if (archive->inbufp+archive->inbufc>=archive->inbufa) {
    if (archive->inbufp) {
      memmove(archive->inbuf,archive->inbuf+archive->inbufp,archive->inbufc);
      archive->inbufp=0;
    } else {
      if (archive->inbufa>=RB_ARCHIVE_INBUF_LIMIT) return -1;
      int na=archive->inbufa+4096;
      void *nv=realloc(archive->inbuf,na);
      if (!nv) return -1;
      archive->inbuf=nv;
      archive->inbufa=na;
    }
  }
  
  int err=read(archive->fd,archive->inbuf+archive->inbufp+archive->inbufc,archive->inbufa-archive->inbufc-archive->inbufp);
  if (err<=0) {
    close(archive->fd);
    archive->fd=-1;
    return 0;
  }
  archive->inbufc+=err;
  return 0;
}

/* Grow output buffer.
 */
 
static int rb_archive_require_resbuf(struct rb_archive *archive) {
  if (archive->resbufp+archive->resbufc<archive->resbufa) return 0;
  if (archive->resbufp) {
    memmove(archive->resbuf,archive->resbuf+archive->resbufp,archive->resbufc);
    archive->resbufp=0;
    return 0;
  }
  if (archive->resbufa>=RB_ARCHIVE_RESBUF_LIMIT) return -1;
  int na=archive->resbufa+4096;
  void *nv=realloc(archive->resbuf,na);
  if (!nv) return -1;
  archive->resbuf=nv;
  archive->resbufa=na;
  return 0;
}

/* Advance the stream state and fail if progress is not possible.
 */
 
static int rb_archive_next(struct rb_archive *archive) {
  int err;
  
  // Decode as much as possible from what's already decompressed.
  if (err=rb_archive_decode(archive)) return err;
  
  // If decompression is complete, decoding must be complete too.
  // If not, the file is malformed (eg an unfinished VLQ).
  if (!archive->zinit) {
    if (archive->resbufc) return -1;
    return 0;
  }
  
  // Acquire input for zlib, and ensure output space for it.
  if (rb_archive_fill_inbuf(archive)<0) return -1;
  if (rb_archive_require_resbuf(archive)<0) return -1;
  archive->z.next_in=(Bytef*)(archive->inbuf+archive->inbufp);
  archive->z.avail_in=archive->inbufc;
  archive->z.next_out=(Bytef*)(archive->resbuf+archive->resbufp+archive->resbufc);
  archive->z.avail_out=archive->resbufa-archive->resbufc-archive->resbufp;
  if (!archive->z.avail_in&&!archive->z.avail_out) {
    // Pretty sure this can't happen but it's easy to check.
    return -1;
  }
  
  // Advance the decompressor.
  int mode=Z_NO_FLUSH;
  if (!archive->inbufc) mode=Z_FINISH;
  if ((err=inflate(&archive->z,mode))<0) {
    return -1;
  }
  int inrmc=archive->inbufc-archive->z.avail_in;
  if (archive->inbufc-=inrmc) archive->inbufp+=inrmc;
  else archive->inbufp=0;
  int outaddc=archive->resbufa-archive->resbufc-archive->resbufp-archive->z.avail_out;
  archive->resbufc+=outaddc;
  if ((mode==Z_FINISH)&&(err==Z_STREAM_END)) {
    inflateEnd(&archive->z);
    archive->zinit=0;
  }
  
  return 0;
}

/* Read archive, main entry point.
 */
 
int rb_archive_read(
  const char *path,
  int (*cb_res)(uint32_t type,int id,const void *src,int srcc,void *userdata),
  void *userdata
) {
  if (!path||!cb_res) return -1;
  struct rb_archive archive={
    .path=path,
    .cb_res=cb_res,
    .userdata=userdata,
    .fd=-1,
  };
  if (rb_archive_open(&archive)<0) {
    rb_archive_cleanup(&archive);
    return -1;
  }
  while (!rb_archive_finished(&archive)) {
    int err=rb_archive_next(&archive);
    if (err) {
      rb_archive_cleanup(&archive);
      return err;
    }
  }
  rb_archive_cleanup(&archive);
  return 0;
}
