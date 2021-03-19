#include "rb_cli.h"
#include "aucm/rb_aucm.h"
#include "rabbit/rb_fs.h"

/* Compile memory to memory.
 */
 
static int rb_synthc(void *dstpp,const char *src,int srcc,struct rb_cli *cli) {

  const char *base=cli->datapath;
  int basec=0,p=0;
  for (;cli->datapath[p];p++) {
    if (cli->datapath[p]=='/') {
      base=cli->datapath+p+1;
      basec=0;
    } else {
      basec++;
    }
  }
  int id=rb_id_from_basename(base,basec);
  if (id<0) {
    fprintf(stderr,"%s: Unable to guess resource ID. Base name should begin with decimal ID.\n",cli->datapath);
    return -1;
  }

  int ftype=rb_ftype_from_path(cli->datapath,-1);
  switch (ftype) {
  
    case RB_FTYPE_INS: {
        if (id>0x7f) {
          fprintf(stderr,"%s: Instrument ID must be in 0..127\n",cli->datapath);
          return -1;
        }
        return rb_instrument_compile(dstpp,src,srcc,id,cli->datapath);
      }
      
    case RB_FTYPE_SND: {
        int programid=id/1000;
        int noteid=id%1000;
        if ((programid>0x7f)||(noteid>0x7f)) {
          fprintf(stderr,"%s: Instrument and note ID must be in 0..127; concatenate them decimally.\n",cli->datapath);
          return -1;
        }
        return rb_sound_compile(dstpp,src,srcc,programid,noteid,0,0,cli->datapath);
      }
      
    default: {
        fprintf(stderr,"%s: Unable to guess file type. Should end '.ins' or '.snd'\n",cli->datapath);
        return -1;
      }
  }
  return 0;
}

/* Main entry point.
 */
 
int rb_cli_main_synthc(struct rb_cli *cli) {
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
  int dstc=rb_synthc(&dst,src,srcc,cli);
  free(src);
  if (dstc<0) {
    fprintf(stderr,"%s:ERROR: Failed to compile\n",cli->datapath);
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
