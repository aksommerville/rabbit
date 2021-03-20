#include "rb_cli.h"
#include "rabbit/rb_fs.h"
#include "rabbit/rb_serial.h"
#include "rabbit/rb_archive.h"
#include "aucm/rb_aucm.h"
#include <errno.h>
#include <zlib.h>

// Set the high bit on fake res types; that makes them incompatible with real ones.
#define RB_FAKE_RES_TYPE_sound 0x80000001

/* Context.
 */

struct rb_archive_context {
  struct rb_cli *cli;
  struct rb_encoder dst;
  struct rb_res {
    uint32_t type;
    int id;
    void *src;
    int srcc;
  } *resv;
  int resc,resa;
  uint32_t restype; // during directory scan and encode
  int pvid; // during encode
};

static void rb_res_cleanup(struct rb_res *res) {
  if (res->src) free(res->src);
}

static void rb_archive_context_cleanup(struct rb_archive_context *context) {
  rb_encoder_cleanup(&context->dst);
  if (context->resv) {
    while (context->resc-->0) rb_res_cleanup(context->resv+context->resc);
    free(context->resv);
  }
}

/* Search resources.
 */
 
static int rb_cli_archive_search(const struct rb_archive_context *context,uint32_t type,int id) {
  int lo=0,hi=context->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rb_res *res=context->resv+ck;
         if (type<res->type) hi=ck;
    else if (type>res->type) lo=ck+1;
    else if (id<res->id) hi=ck;
    else if (id>res->id) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert resource.
 */
 
static struct rb_res *rb_cli_archive_add_res(struct rb_archive_context *context,uint32_t type,int id) {
  int p=rb_cli_archive_search(context,type,id);
  if (p>=0) return 0;
  p=-p-1;
  if (context->resc>=context->resa) {
    int na=context->resa+32;
    if (na>INT_MAX/sizeof(struct rb_res)) return 0;
    void *nv=realloc(context->resv,sizeof(struct rb_res)*na);
    if (!nv) return 0;
    context->resv=nv;
    context->resa=na;
  }
  struct rb_res *res=context->resv+p;
  memmove(res+1,res,sizeof(struct rb_res)*(context->resc-p));
  context->resc++;
  memset(res,0,sizeof(struct rb_res));
  res->type=type;
  res->id=id;
  return res;
}

/* Receive typed resource file.
 */
 
static int rb_cli_archive_cb_inner(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_archive_context *context=userdata;
  
  int id=rb_id_from_basename(base,basec);
  if (id<0) {
    fprintf(stderr,"%.*s: Unable to determine resource ID. Ignoring file.\n",pathc,path);
    return 0;
  }
  
  void *src=0;
  int srcc=rb_file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%.*s: Failed to read file\n",pathc,path);
    return -1;
  }
  
  struct rb_res *res=rb_cli_archive_add_res(context,context->restype,id);
  if (!res) {
    fprintf(stderr,"%.*s: Failed to insert resource %08x:%d. Is there a duplicate?\n",pathc,path,context->restype,id);
    free(src);
    return -1;
  }
  
  res->src=src; // HANDOFF
  res->srcc=srcc;
  
  return 0;
}

/* Receive file in outer directory.
 * These should be directories named for a specific resource type.
 */
 
static int rb_cli_archive_cb_outer(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_archive_context *context=userdata;
  
  if (!type) type=rb_file_get_type(path);
  if (type!='d') {
    if ((basec==4)&&!memcmp(base,"plan",4)) return 0; // well, we know this one
    fprintf(stderr,"%.*s: Unexpected file type in outer data directory, ignoring.\n",pathc,path);
    return 0;
  }
  
       if ((basec== 5)&&!memcmp(base,"image",      5)) context->restype=RB_RES_TYPE_imag;
  else if ((basec==10)&&!memcmp(base,"instrument",10)) context->restype=RB_RES_TYPE_snth;
  else if ((basec== 5)&&!memcmp(base,"sound",      5)) context->restype=RB_FAKE_RES_TYPE_sound;
  else if ((basec== 4)&&!memcmp(base,"song",       4)) context->restype=RB_RES_TYPE_song;
  else if ((basec== 4)&&!memcmp(base,"grid",       4)) context->restype=RB_RES_TYPE_grid;
  else if ((basec== 4)&&!memcmp(base,"text",       4)) context->restype=RB_RES_TYPE_text;
  else if ((basec== 4)&&!memcmp(base,"data",       4)) context->restype=RB_RES_TYPE_data;
  else {
    fprintf(stderr,"%.*s: Unknown resource type. Will output as 'data'\n",pathc,path);
    context->restype=RB_RES_TYPE_data;
  }
  
  return rb_dir_read(path,rb_cli_archive_cb_inner,context);
}

/* Locate all "sound" resources and pack them into the appropriate "instrument".
 * Creates instruments as needed.
 */
 
static int rb_archive_pack_sounds(struct rb_archive_context *context) {
  while (1) {
  
    int soundp=rb_cli_archive_search(context,RB_FAKE_RES_TYPE_sound,0);
    if (soundp<0) soundp=-soundp-1;
    if (soundp>=context->resc) break;
    if (context->resv[soundp].type!=RB_FAKE_RES_TYPE_sound) break;
    struct rb_res sound=context->resv[soundp]; // HANDOFF
    context->resc--;
    memmove(context->resv+soundp,context->resv+soundp+1,sizeof(struct rb_res)*(context->resc-soundp));
    
    int programid=sound.id/1000;
    int noteid=sound.id%1000;
    if ((programid<0)||(programid>127)||(noteid<0)||(noteid>127)) {
      fprintf(stderr,"Invalid sound id %d\n",sound.id);
      rb_res_cleanup(&sound);
      return -1;
    }
    
    // If we don't have the instrument yet, easy, use sound as is. Sounds are complete archives.
    int snthp=rb_cli_archive_search(context,RB_RES_TYPE_snth,programid);
    if (snthp<0) {
      struct rb_res *nres=rb_cli_archive_add_res(context,RB_RES_TYPE_snth,programid);
      if (!nres) {
        rb_res_cleanup(&sound);
        return -1;
      }
      nres->src=sound.src; // HANDOFF
      nres->srcc=sound.srcc;
      continue;
    }
    
    struct rb_res *snth=context->resv+snthp;
    struct rb_encoder combined={0};
    if (rb_multiplex_combine_archives(&combined,snth->src,snth->srcc,sound.src,sound.srcc)<0) {
      rb_res_cleanup(&sound);
      rb_encoder_cleanup(&combined);
      return -1;
    }
    if (snth->src) free(snth->src);
    snth->src=combined.v; // HANDOFF
    snth->srcc=combined.c;
    
    rb_res_cleanup(&sound);
  }
  return 0;
}

/* I kind of botched this... aucm emits an archive for each program.
 * That's cool and all, keeps them full, loadable packages.
 * But when combining into the master archive, we should strip them each to just the inner program.
 */
 
static int rb_strip_synth_archive_header(int id,uint8_t *serial,int serialc) {

  int serialp=0;
  if (serialp>=serialc) return 0; // empty is ok
  int programid=serial[serialp++];
  if (id!=programid) return -1;
  
  int len,err;
  if ((err=rb_vlq_decode(&len,serial+serialp,serialc-serialp))<1) return -1;
  serialp+=err;
  if (serialp+len!=serialc) return -1;
  
  memmove(serial,serial+serialp,len);
  return len;
}
 
static int rb_archive_strip_synth_archives(struct rb_archive_context *context) {
  int p=rb_cli_archive_search(context,RB_RES_TYPE_snth,0);
  if (p<0) p=-p-1;
  for (;p<context->resc;p++) {
    struct rb_res *res=context->resv+p;
    if (res->type!=RB_RES_TYPE_snth) break;
    int err=rb_strip_synth_archive_header(res->id,res->src,res->srcc);
    if (err<0) return -1;
    res->srcc=err;
  }
  return 0;
}

/* After reading the initial set of resources, perform any cross-checking, filtering, combination, etc.
 * Last chance to modify (resv) before we encode it.
 */
 
static int rb_archive_digest(struct rb_archive_context *context) {
  if (rb_archive_pack_sounds(context)<0) return -1;
  if (rb_archive_strip_synth_archives(context)<0) return -1;
  //TODO other digest ops?
  return 0;
}

/* Compress and append a chunk of data.
 */
 
static int rb_archive_append_output(
  struct rb_archive_context *context,
  const void *src,int srcc,
  z_stream *z
) {
  z->next_in=(Bytef*)src;
  z->avail_in=srcc;
  while (z->avail_in>0) {
    if (rb_encoder_require(&context->dst,1024)<0) return -1;
    z->next_out=(Bytef*)(context->dst.v+context->dst.c);
    z->avail_out=context->dst.a-context->dst.c;
    if (deflate(z,Z_NO_FLUSH)<0) return -1;
    context->dst.c+=context->dst.a-context->dst.c-z->avail_out;
  }
  return 0;
}

/* Append one resource to the archive.
 */
 
static int rb_archive_encode_res(
  struct rb_archive_context *context,
  const struct rb_res *res,
  z_stream *z
) {
  
  char hdr[12];
  int hdrc=0,err;
  if (res->type!=context->restype) {
    hdr[hdrc++]=0x80|(res->type>>24);
    hdr[hdrc++]=0x80|(res->type>>16);
    hdr[hdrc++]=0x80|(res->type>>8);
    hdr[hdrc++]=0x80|res->type;
    context->restype=res->type;
    context->pvid=0;
  }
  int idd=res->id-context->pvid;
  context->pvid=res->id;
  if ((err=rb_vlq_encode(hdr+hdrc,12-hdrc,idd))<1) return -1;
  hdrc+=err;
  if ((err=rb_vlq_encode(hdr+hdrc,12-hdrc,res->srcc))<1) return -1;
  hdrc+=err;
  
  //fprintf(stderr,"OUTPUT: %08x:%d %d\n",res->type,res->id,res->srcc);
  if (rb_archive_append_output(context,hdr,hdrc,z)<0) return -1;
  if (rb_archive_append_output(context,res->src,res->srcc,z)<0) return -1;
  
  return 0;
}

/* Populate (context->dst) with the final compressed archive.
 */
 
static int rb_archive_encode(struct rb_archive_context *context) {
  context->dst.c=0;
  z_stream z={0};
  if (deflateInit(&z,Z_BEST_COMPRESSION)<0) return -1;
  context->restype=0;
  context->pvid=0;
  
  const struct rb_res *res=context->resv;
  int i=context->resc;
  for (;i-->0;res++) {
    if (rb_archive_encode_res(context,res,&z)<0) {
      deflateEnd(&z);
      return -1;
    }
  }
  
  while (1) {
    if (rb_encoder_require(&context->dst,1024)<0) {
      deflateEnd(&z);
      return -1;
    }
    z.next_out=(Bytef*)(context->dst.v+context->dst.c);
    z.avail_out=context->dst.a-context->dst.c;
    int err=deflate(&z,Z_FINISH);
    if (err<0) {
      deflateEnd(&z);
      return -1;
    }
    context->dst.c+=context->dst.a-context->dst.c-z.avail_out;
    if (err==Z_STREAM_END) break;
  }
  
  deflateEnd(&z);
  return 0;
}

/* Main entry point.
 */
 
int rb_cli_main_archive(struct rb_cli *cli) {
  struct rb_archive_context context={
    .cli=cli,
  };
  
  if (!cli->dstpath||!cli->dstpath[0]) {
    fprintf(stderr,"%s: '--dst=PATH' required\n",cli->exename);
    return -1;
  }
  
  if (rb_dir_read(cli->datapath,rb_cli_archive_cb_outer,&context)<0) {
    rb_archive_context_cleanup(&context);
    return -1;
  }
  
  if (rb_archive_digest(&context)<0) {
    rb_archive_context_cleanup(&context);
    return -1;
  }
  
  if (rb_archive_encode(&context)<0) {
    rb_archive_context_cleanup(&context);
    return -1;
  }
  
  if (rb_file_write(cli->dstpath,context.dst.v,context.dst.c)<0) {
    fprintf(stderr,"%s: Failed to write archive\n",cli->exename);
    rb_archive_context_cleanup(&context);
    return -1;
  }

  rb_archive_context_cleanup(&context);
  return 0;
}
