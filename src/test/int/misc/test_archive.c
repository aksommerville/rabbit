#include "test/rb_test.h"
#include "rabbit/rb_archive.h"
#include "rabbit/rb_fs.h"
#include <zlib.h>

/* Helper to generate an archive file from formatted but uncompressed data.
 */
 
static int generate_1(const char *path,const void *src,int srcc) {
  int dstc=0,dsta=256;
  char *dst=malloc(dsta);
  RB_ASSERT(dst)
  z_stream z={0};
  RB_ASSERT_CALL(deflateInit(&z,Z_BEST_COMPRESSION))
  z.next_in=(Bytef*)src;
  z.avail_in=srcc;
  int done=0;
  while (!done) {
    if (dstc>=dsta) {
      dsta+=256;
      dst=realloc(dst,dsta);
      RB_ASSERT(dst)
    }
    z.next_out=(Bytef*)(dst+dstc);
    z.avail_out=dsta-dstc;
    int mode=(z.avail_in?Z_NO_FLUSH:Z_FINISH);
    int err=deflate(&z,mode);
    RB_ASSERT_CALL(err,"deflate()")
    dstc+=(dsta-dstc-z.avail_out);
    if (err==Z_STREAM_END) done=1;
  }
  deflateEnd(&z);
  RB_ASSERT_CALL(rb_file_write(path,dst,dstc),"path=%s dstc=%d srcc=%d",path,dstc,srcc)
  free(dst);
  return 0;
}

/* Generate test data.
 * I'm going to commit the test data too, so we should only need this once.
 */
 
XXX_RB_ITEST(generate_archive_test_data) {
  
  {
    const uint8_t raw[]={
      'h'|0x80,'e'|0x80,'l'|0x80,'o'|0x80, // type change
      0x03, // id 3
      0x05, // length 5
      'D','a','t','a','!',
      0x02, // id + 2 = 5
      0x02, // length 2
      'O','K',
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-simple.res",raw,sizeof(raw)))
  }
  
  {
    const uint8_t raw[]={
      'h'|0x80,'e'|0x80,'l'|0x80,'o'|0x80, // type change
      0x80,0x03, // id 3
      0x80,0x80,0x05, // length 5
      'N','o','P','r','b',
      0x80,0x80,0x80,0x02, // id + 2 = 5
      0x80,0x80,0x80,0x02, // length 2
      'S','i',
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-overencode.res",raw,sizeof(raw)))
  }
  
  {
    const uint8_t raw[]={
      // type implicitly zero
      0x00, // id 0
      0x08, // length 8
      'D','e','f','a','u','l','t','s',
      0x00, // id also 0, weird but legal
      0x03, // length 3
      'O','K','?',
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-defaults.res",raw,sizeof(raw)))
  }
  
  {
    const uint8_t raw[]={
      'T'|0x80,'y'|0x80,'p'|0x80,'1'|0x80,
      0x04, // id 4
      0x05, // length 5
      'T','y','p','e','1',
      'T'|0x80,'y'|0x80,'p'|0x80,'2'|0x80,
      0x02, // id 2
      0x05, // length 5
      'T','y','p','e','2',
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-multi-type.res",raw,sizeof(raw)))
  }
  
  {
    const uint8_t raw[]={
      'd'|0x80,'a'|0x80,'t'|0x80,'a'|0x80,
      0x04, // id 4
      0x05, // length 5
      'f','o','u','r', // oh no that's not five :(
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-short-body.res",raw,sizeof(raw)))
  }
  
  {
    const uint8_t raw[]={
      'd'|0x80,'a'|0x80,'t'|0x80,'a'|0x80,
      0x04, // id 4
      0x80,0x80,0x80, // length incomplete :(
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-short-length.res",raw,sizeof(raw)))
  }
  
  {
    const uint8_t raw[]={
      'd'|0x80,'a'|0x80,'t'|0x80,'a'|0x80,
      0x84, // id incomplete :(
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-short-id.res",raw,sizeof(raw)))
  }
  
  {
    const uint8_t raw[]={
      'f'|0x80,'a'|0x80,'k'|0x80,'e'|0x80,
      'r'|0x80,'e'|0x80,'a'|0x80,'l'|0x80,
      0x01, // id 1
      0x08, // length 8
      'h','i',' ','w','o','r','l','d',
    };
    RB_ASSERT_CALL(generate_1("src/test/int/misc/archive-empty-type.res",raw,sizeof(raw)))
  }
  
  return 0;
}

/* archive-simple.res:
 *   helo:3: "Data!"
 *   helo:5: "OK"
 */
 
static int cb_decode_archive_simple(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  int *count=userdata;
  if (*count==0) {
    RB_ASSERT_INTS(restype,RB_RES_TYPE('h','e','l','o'))
    RB_ASSERT_INTS(resid,3)
    RB_ASSERT_STRINGS(src,srcc,"Data!",5)
  } else if (*count==1) {
    RB_ASSERT_INTS(restype,RB_RES_TYPE('h','e','l','o'))
    RB_ASSERT_INTS(resid,5)
    RB_ASSERT_STRINGS(src,srcc,"OK",2)
  }
  (*count)++;
  return 0;
}
 
RB_ITEST(decode_archive_simple) {
  int count=0;
  RB_ASSERT_INTS(0,rb_archive_read(
    "src/test/int/misc/archive-simple.res",
    cb_decode_archive_simple,&count
  ))
  RB_ASSERT_INTS(count,2)
  return 0;
}

/* archive-overencode.res:
 *   helo:3: "NoPrb"
 *   helo:5: "Si"
 */
 
static int cb_decode_archive_overencode(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  int *count=userdata;
  if (*count==0) {
    RB_ASSERT_INTS(restype,RB_RES_TYPE('h','e','l','o'))
    RB_ASSERT_INTS(resid,3)
    RB_ASSERT_STRINGS(src,srcc,"NoPrb",5)
  } else if (*count==1) {
    RB_ASSERT_INTS(restype,RB_RES_TYPE('h','e','l','o'))
    RB_ASSERT_INTS(resid,5)
    RB_ASSERT_STRINGS(src,srcc,"Si",2)
  }
  (*count)++;
  return 0;
}
 
RB_ITEST(decode_archive_overencode) {
  int count=0;
  RB_ASSERT_INTS(0,rb_archive_read(
    "src/test/int/misc/archive-overencode.res",
    cb_decode_archive_overencode,&count
  ))
  RB_ASSERT_INTS(count,2)
  return 0;
}

/* archive-defaults.res:
 *   0:0: "Defaults"
 *   0:0: "OK?"
 */
 
static int cb_decode_archive_defaults(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  int *count=userdata;
  if (*count==0) {
    RB_ASSERT_INTS(restype,0)
    RB_ASSERT_INTS(resid,0)
    RB_ASSERT_STRINGS(src,srcc,"Defaults",8)
  } else if (*count==1) {
    RB_ASSERT_INTS(restype,0)
    RB_ASSERT_INTS(resid,0)
    RB_ASSERT_STRINGS(src,srcc,"OK?",3)
  }
  (*count)++;
  return 0;
}
 
RB_ITEST(decode_archive_defaults) {
  int count=0;
  RB_ASSERT_INTS(0,rb_archive_read(
    "src/test/int/misc/archive-defaults.res",
    cb_decode_archive_defaults,&count
  ))
  RB_ASSERT_INTS(count,2)
  return 0;
}

/* archive-multi-type.res:
 *   Typ1:4: "Type1"
 *   Typ2:2: "Type2"
 */
 
static int cb_decode_archive_multitype(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  int *count=userdata;
  if (*count==0) {
    RB_ASSERT_INTS(restype,RB_RES_TYPE('T','y','p','1'))
    RB_ASSERT_INTS(resid,4)
    RB_ASSERT_STRINGS(src,srcc,"Type1",5)
  } else if (*count==1) {
    RB_ASSERT_INTS(restype,RB_RES_TYPE('T','y','p','2'))
    RB_ASSERT_INTS(resid,2)
    RB_ASSERT_STRINGS(src,srcc,"Type2",5)
  }
  (*count)++;
  return 0;
}
 
RB_ITEST(decode_archive_multitype) {
  int count=0;
  RB_ASSERT_INTS(0,rb_archive_read(
    "src/test/int/misc/archive-multi-type.res",
    cb_decode_archive_multitype,&count
  ))
  RB_ASSERT_INTS(count,2)
  return 0;
}

/* archive-short-body.res
 */
 
static int cb_decode_archive_short_body(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  *(int*)userdata=1;
  RB_FAIL("unexpected resource %d:%d, %d bytes",restype,resid,srcc)
}
 
RB_ITEST(decode_archive_short_body) {
  int realerror=0;
  RB_ASSERT_FAILURE(rb_archive_read(
    "src/test/int/misc/archive-short-body.res",
    cb_decode_archive_short_body,&realerror
  ))
  RB_ASSERT_NOT(realerror)
  return 0;
}

/* archive-short-length.res
 */
 
static int cb_decode_archive_short_length(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  *(int*)userdata=1;
  RB_FAIL("unexpected resource %d:%d, %d bytes",restype,resid,srcc)
}
 
RB_ITEST(decode_archive_short_length) {
  int realerror=0;
  RB_ASSERT_FAILURE(rb_archive_read(
    "src/test/int/misc/archive-short-length.res",
    cb_decode_archive_short_length,&realerror
  ))
  RB_ASSERT_NOT(realerror)
  return 0;
}

/* archive-short-id.res
 */
 
static int cb_decode_archive_short_id(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  *(int*)userdata=1;
  RB_FAIL("unexpected resource %d:%d, %d bytes",restype,resid,srcc)
}
 
RB_ITEST(decode_archive_short_id) {
  int realerror=0;
  RB_ASSERT_FAILURE(rb_archive_read(
    "src/test/int/misc/archive-short-id.res",
    cb_decode_archive_short_id,&realerror
  ))
  RB_ASSERT_NOT(realerror)
  return 0;
}

/* archive-empty-type.res:
 *   real:1: "hi world"
 */
 
static int cb_decode_archive_empty_type(uint32_t restype,int resid,const void *src,int srcc,void *userdata) {
  int *count=userdata;
  if (*count==0) {
    RB_ASSERT_INTS(restype,RB_RES_TYPE('r','e','a','l'))
    RB_ASSERT_INTS(resid,1)
    RB_ASSERT_STRINGS(src,srcc,"hi world",8)
  }
  (*count)++;
  return 0;
}
 
RB_ITEST(decode_archive_empty_type) {
  int count=0;
  RB_ASSERT_INTS(0,rb_archive_read(
    "src/test/int/misc/archive-empty-type.res",
    cb_decode_archive_empty_type,&count
  ))
  RB_ASSERT_INTS(count,1)
  return 0;
}
