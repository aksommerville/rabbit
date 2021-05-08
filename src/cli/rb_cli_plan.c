#include "rb_cli.h"
#include "rabbit/rb_fs.h"
#include <errno.h>

/* Context.
 */

struct rb_plan_context {
  struct rb_cli *cli;
  FILE *dstf;
  int srcdirlen;
};

static void rb_plan_context_cleanup(struct rb_plan_context *context) {
  if (context->dstf!=stdout) {
    fclose(context->dstf);
  }
}

/* Receive instrument file.
 */
 
static int rb_cli_plan_cb_instrument(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_plan_context *context=userdata;
  const char *subpath=path+context->srcdirlen;
  int subpathc=pathc-context->srcdirlen;
  
  fprintf(context->dstf,"DATAMIDFILES+=$(DATAMIDDIR)/%.*s\n",subpathc,subpath);
  fprintf(context->dstf,
    "$(DATAMIDDIR)/%.*s:%.*s $(EXE_CLI);$(PRECMD) $(EXE_CLI) synthc --dst=$@ --data=$<\n",
    subpathc,subpath,pathc,path
  );
  
  return 0;
}

/* Receive sound file.
 */
 
static int rb_cli_plan_cb_sound(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_plan_context *context=userdata;
  const char *subpath=path+context->srcdirlen;
  int subpathc=pathc-context->srcdirlen;
  
  fprintf(context->dstf,"DATAMIDFILES+=$(DATAMIDDIR)/%.*s\n",subpathc,subpath);
  fprintf(context->dstf,
    "$(DATAMIDDIR)/%.*s:%.*s $(EXE_CLI);$(PRECMD) $(EXE_CLI) synthc --dst=$@ --data=$<\n",
    subpathc,subpath,pathc,path
  );
  
  return 0;
}

/* Receive song file.
 */
 
static int rb_cli_plan_cb_song(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_plan_context *context=userdata;
  const char *subpath=path+context->srcdirlen;
  int subpathc=pathc-context->srcdirlen;
  int dstsubpathc=subpathc;
  if ((subpathc>=4)&&!memcmp(subpath+subpathc-4,".mid",4)) dstsubpathc-=4;
  
  fprintf(context->dstf,"DATAMIDFILES+=$(DATAMIDDIR)/%.*s.song\n",dstsubpathc,subpath);
  fprintf(context->dstf,"$(DATAMIDDIR)/%.*s.song:%.*s;$(PRECMD) $(EXE_CLI) songc --dst=$@ --data=$<\n",dstsubpathc,subpath,pathc,path);
  
  return 0;
}

/* Receive image file.
 */
 
static int rb_cli_plan_cb_image(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_plan_context *context=userdata;
  const char *subpath=path+context->srcdirlen;
  int subpathc=pathc-context->srcdirlen;
  
  while ((subpathc>=1)&&(subpath[0]=='/')) { subpath++; subpathc--; }
  
  fprintf(context->dstf,"DATAMIDFILES+=$(DATAMIDDIR)/%.*s\n",subpathc,subpath);
  fprintf(context->dstf,"$(DATAMIDDIR)/%.*s:%.*s $(EXE_CLI);$(PRECMD) $(EXE_CLI) imagec --dst=$@ --data=$<\n",subpathc,subpath,pathc,path);
  
  return 0;
}

/* Receive file in outer directory.
 * These should be directories named for a specific resource type.
 */
 
static int rb_cli_plan_cb_outer(
  const char *path,int pathc,
  const char *base,int basec,
  char type,
  void *userdata
) {
  struct rb_plan_context *context=userdata;
  
  if (!type) type=rb_file_get_type(path);
  if (type!='d') {
    fprintf(stderr,"%.*s: Unexpected file type in outer data directory, ignoring.\n",pathc,path);
    return 0;
  }
  
  if ((basec==10)&&!memcmp(base,"instrument",10)) {
    return rb_dir_read(path,rb_cli_plan_cb_instrument,context);
  }
  if ((basec==5)&&!memcmp(base,"sound",5)) {
    return rb_dir_read(path,rb_cli_plan_cb_sound,context);
  }
  if ((basec==4)&&!memcmp(base,"song",4)) {
    return rb_dir_read(path,rb_cli_plan_cb_song,context);
  }
  if ((basec==5)&&!memcmp(base,"image",5)) {
    return rb_dir_read(path,rb_cli_plan_cb_image,context);
  }
  
  fprintf(stderr,"%.*s: Unexpected directory in outer data directory, ignoring.\n",pathc,path);
  return 0;
}

/* Main entry point.
 */
 
int rb_cli_main_plan(struct rb_cli *cli) {
  struct rb_plan_context context={
    .cli=cli,
    .dstf=stdout,
  };
  
  while (cli->datapath[context.srcdirlen]) context.srcdirlen++;

  if (cli->dstpath&&cli->dstpath[0]) {
    if (!(context.dstf=fopen(cli->dstpath,"wb"))) {
      fprintf(stderr,"%s: %s\n",cli->dstpath,strerror(errno));
      return -1;
    }
  }
  
  fprintf(context.dstf,"ifndef DATAMIDDIR\n$(error Please define DATAMIDDIR)\nendif\n");
  fprintf(context.dstf,"ifndef EXE_CLI\n$(error Please define EXE_CLI)\nendif\n");
  fprintf(context.dstf,"DATAMIDFILES:=\n");
  
  if (rb_dir_read(cli->datapath,rb_cli_plan_cb_outer,&context)<0) {
    rb_plan_context_cleanup(&context);
    return -1;
  }
  
  fprintf(context.dstf,"$(DATADST):$(DATAMIDFILES) $(EXE_CLI);$(PRECMD) $(EXE_CLI) archive --dst=$@ --data=$(DATAMIDDIR)\n");

  rb_plan_context_cleanup(&context);
  return 0;
}
