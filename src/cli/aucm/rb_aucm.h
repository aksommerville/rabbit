/* rb_aucm.h
 * Audio compiler.
 * See etc/doc/audio-text.txt
 */
 
#ifndef RB_AUCM_H
#define RB_AUCM_H

struct rb_encoder;

#define RB_CM_FULLY_LOGGED -2

/* Produce an encoded synth archive containing one program.
 * Returns RB_CM_FULLY_LOGGED if we printed enough to stderr, or -1 for other errors.
 */
int rb_instrument_compile(
  void *dstpp,
  const char *src,int srcc,
  uint8_t programid,
  const char *path
);

/* Produce an encoded synth archive containing one multiplex program.
 * Returns RB_CM_FULLY_LOGGED if we printed enough to stderr, or -1 for other errors.
 * (program) is empty or the previous encoded content.
 * (src) describes just one note of the multiplex program.
 */
int rb_sound_compile(
  void *dstpp,
  const char *src,int srcc,
  uint8_t programid,
  uint8_t noteid,
  const void *program,
  int programc,
  const char *path
);

/* Support for combining multiplex programs.
 * In all cases, (b) overrides (a).
 * "add_archive_to_program" sounds backward, but is in fact the way aucm needs it.
 * These are only guaranteed to produce valid output for single-note ranges; that's the only thing we use them for.
 * We fail if any fields exist other than principal ranges.
 */
int rb_multiplex_combine_archives(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc); // complete archives
int rb_multiplex_combine_programs(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc); // from ntid on
int rb_multiplex_combine_fields(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc); // just the principal field content
int rb_multiplex_add_archive_to_program(struct rb_encoder *dst,const void *a,int ac,const void *b,int bc); // emits archive

#endif
