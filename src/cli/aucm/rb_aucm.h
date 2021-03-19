/* rb_aucm.h
 * Audio compiler.
 * See etc/doc/audio-text.txt
 */
 
#ifndef RB_AUCM_H
#define RB_AUCM_H

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

#endif
