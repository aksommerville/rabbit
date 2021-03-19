#include "rb_cli.h"
#include "rabbit/rb_fs.h"
#include "rabbit/rb_serial.h"
#include <errno.h>

/* Context.
 */

struct rb_archive_context {
  struct rb_cli *cli;
  struct rb_encoder dst;
};

static void rb_archive_context_cleanup(struct rb_archive_context *context) {
  rb_encoder_cleanup(&context->dst);
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
  
  fprintf(stderr,"%.*s: TODO %s\n",pathc,path,__func__);
  
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
  struct rb_plan_context *context=userdata;
  
  if (!type) type=rb_file_get_type(path);
  if (type!='d') {
    if ((basec==4)&&!memcmp(base,"plan",4)) return 0; // well, we know this one
    fprintf(stderr,"%.*s: Unexpected file type in outer data directory, ignoring.\n",pathc,path);
    return 0;
  }
  
  //TODO Determine resource type from (base).
  
  return rb_dir_read(path,rb_cli_archive_cb_inner,context);
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
  
  if (rb_file_write(cli->dstpath,context.dst.v,context.dst.c)<0) {
    fprintf(stderr,"%s: Failed to write archive\n",cli->exename);
    rb_archive_context_cleanup(&context);
    return -1;
  }

  rb_archive_context_cleanup(&context);
  return 0;
}
