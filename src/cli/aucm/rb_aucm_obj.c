#include "rb_aucm_internal.h"
#include <stdarg.h>

/* Cleanup.
 */
 
void rb_aucm_cleanup(struct rb_aucm *aucm) {
  if (aucm->message) free(aucm->message);
  rb_encoder_cleanup(&aucm->bin);
}

/* Set error message.
 */

int rb_aucm_error(struct rb_aucm *aucm,const char *fmt,...) {
  if (aucm->help) return RB_CM_FULLY_LOGGED;
  if (aucm&&!aucm->messagec&&fmt&&fmt[0]) {
    va_list vargs;
    va_start(vargs,fmt);
    char tmp[256];
    int tmpc=vsnprintf(tmp,sizeof(tmp),fmt,vargs);
    if ((tmpc>0)&&(tmpc<sizeof(tmp))) {
      char *nv=malloc(tmpc+1);
      if (!nv) return -1;
      memcpy(nv,tmp,tmpc+1);
      if (aucm->message) free(aucm->message);
      aucm->message=nv;
      aucm->messagec=tmpc;
    }
  }
  return -1;
}

/* Print message to stderr.
 */
 
static void rb_aucm_log_failure(struct rb_aucm *aucm) {

  if (aucm->messagec) {
    fprintf(stderr,
      "%s:%d:ERROR: %.*s\n",
      aucm->path,aucm->token.lineno,aucm->messagec,aucm->message
    );
  } else {
    fprintf(stderr,
      "%s:%d:ERROR: (no detail available)\n",
      aucm->path,aucm->token.lineno
    );
  }
  
  // Locate the offending line.
  const int LINE_LENGTH_LIMIT=80;
  char tmp[LINE_LENGTH_LIMIT];
  int lineno=1,srcp=0;
  while (lineno<aucm->token.lineno) {
    while ((srcp<aucm->srcc)&&(aucm->src[srcp]!=0x0a)) srcp++;
    if (srcp>=aucm->srcc) {
      lineno=-1;
      break;
    }
    lineno++;
    srcp++;
  }
  const char *line=aucm->src+srcp;
  int linec=0;
  if (lineno==aucm->token.lineno) {
    while ((srcp+linec<aucm->srcc)&&(line[linec]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (linec>LINE_LENGTH_LIMIT) {
      int tokp=aucm->token.v-line;
      if (tokp<0) tokp=0;
      else if (tokp>linec) tokp=linec;
      int np=tokp-(LINE_LENGTH_LIMIT>>1);
      if (np<0) np=0;
      else if (np>linec-LINE_LENGTH_LIMIT) np=linec-LINE_LENGTH_LIMIT;
      line+=np;
      linec=LINE_LENGTH_LIMIT;
    }
    if (linec>0) {
      fprintf(stderr,"  %.*s\n",linec,line);
      int tokp=aucm->token.v-line;
      int tokc=0;
      if (tokp<0) tokp=0;
      else if (tokp>=linec) tokp=linec;
      else if (tokp+aucm->token.c>linec) tokc=linec-tokp;
      else tokc=aucm->token.c;
      memset(tmp,' ',tokp);
      fprintf(stderr,"  %.*s^",tokp,tmp);
      if (tokc>=2) {
        memset(tmp,'-',tokc-2);
        fprintf(stderr,"%.*s^\n",tokc-2,tmp);
      } else {
        fprintf(stderr,"\n");
      }
    }
  }
}

/* Compile, main entry point.
 */
 
int rb_aucm_compile(void *dstpp,struct rb_aucm *aucm) {
  
  aucm->token.v=aucm->src;
  aucm->token.c=0;
  aucm->token.lineno=1;
  aucm->bin.c=0;
  aucm->messagec=0;
  aucm->help=0;
  
  int err=rb_aucm_compile_internal(aucm);
  if (err>=0) {
    *(void**)dstpp=aucm->bin.v;
    int dstc=aucm->bin.c;
    memset(&aucm->bin,0,sizeof(struct rb_encoder));
    return dstc;
  }
  if (aucm->help||(err==RB_CM_FULLY_LOGGED)) return RB_CM_FULLY_LOGGED;
  rb_aucm_log_failure(aucm);
  return RB_CM_FULLY_LOGGED;
}
