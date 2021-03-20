#include "rb_aucm_internal.h"
#include "rabbit/rb_serial.h"

/* Compile instrument.
 */
 
int rb_instrument_compile(
  void *dstpp,
  const char *src,int srcc,
  uint8_t programid,
  const char *path
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  struct rb_aucm aucm={
    .src=src,
    .srcc=srcc,
    .path=path?path:"<unknown>",
    .scope={
      .programid=programid,
    },
  };
  int err=rb_aucm_compile(dstpp,&aucm);
  rb_aucm_cleanup(&aucm);
  return err;
}

/* Compile sound.
 */

int rb_sound_compile(
  void *dstpp,
  const char *src,int srcc,
  uint8_t programid,
  uint8_t noteid,
  const void *program,
  int programc,
  const char *path
) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  const struct rb_synth_node_type *type=&rb_synth_node_type_multiplex;
  struct rb_aucm aucm={
    .src=src,
    .srcc=srcc,
    .path=path?path:"<unknown>",
    .scope={
      .programid=programid,
      .type=type,
      .serialfmt=RB_SYNTH_SERIALFMT_NODES,
      .noteid=noteid,
    },
  };
  void *single=0;
  int singlec=rb_aucm_compile(&single,&aucm);
  rb_aucm_cleanup(&aucm);
  if (singlec<0) return singlec;
  struct rb_encoder output={0};
  if (rb_multiplex_add_archive_to_program(&output,program,programc,single,singlec)<0) {
    rb_encoder_cleanup(&output);
    free(single);
    return -1;
  }
  free(single);
  *(void**)dstpp=output.v;
  return output.c;
}
