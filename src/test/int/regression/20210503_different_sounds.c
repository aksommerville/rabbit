#include "test/rb_test.h"
#include <rabbit/rb_audio.h>
#include <rabbit/rb_synth.h>
#include <rabbit/rb_synth_event.h>
#include <rabbit/rb_pcm_store.h>
#include <rabbit/rb_pcm.h>
#include <rabbit/rb_fs.h>
#include <rabbit/rb_archive.h>
#include <unistd.h>

/* 2021-05-03: When playing Chetyorska, the sound effects sometimes come out funny.
 * Sometimes a particular sound doesn't play at all, and sometimes it's just different.
 * I haven't seen a reliable pattern to the behavior.
 * A sound does remain the same as long as the program is running.
 * So it's in the printers, not the playback. Which is no surprise.
 *
 * First run: Reproduced. I hear two distinct sounds, apparently randomly.
 * This is in one program run, but a fresh audio and synth driver each time.
 *
 * At least 3 different sounds had exactly the same length, 29053.
 *
 * env are different:
 env runner 0x561290543e50
  33204 100.392159 0.000000 : 0 -0.006047 0.000000 0.000000
env runner 0x561290543ef0
  1383 0.148438 0.000000 : 0 0.000107 0.000000 0.000000
  2767 0.018627 0.000000 : 0 -0.000047 0.000000 0.000000
  24903 0.000000 0.000000 : 0 -0.000001 0.000000 51245696.000000
pcm->c=29053
env runner 0x561290546e10
  33204 100.392159 51245696.000000 : 1 0.000000 0.999327 5124569600.000000
env runner 0x561290546eb0
  1383 0.148438 51245696.000000 : 1 -0.000000 0.983964 5124569600.000000
  2767 0.018627 0.000000 : 0 -0.000047 0.000000 0.000000
  24903 0.000000 0.000000 : 0 -0.000001 0.000000 0.000000
pcm->c=29053
 * ...rb_env_decode_long() was reading curve but leaving uninitialized if absent.
 *
 * ok that does seem to fix it.
 */
 
static int cb_archive(uint32_t type,int id,const void *src,int srcc,void *userdata) {
  struct rb_synth *synth=userdata;
  switch (type) {
    case RB_RES_TYPE_snth: if (rb_synth_load_program(synth,id,src,srcc)<0) return -1; break;
  }
  return 0;
}
 
static int cb_pcm_out(int16_t *v,int c,struct rb_audio *audio) {
  struct rb_synth *synth=audio->delegate.userdata;
  if (synth) return rb_synth_update(v,c,synth);
  memset(v,0,c<<1);
  return 0;
}

static void checkpcm(struct rb_pcm *pcm) {
  //fprintf(stderr,"pcm->c=%d\n",pcm->c);
}
 
static int play_sound() {

  struct rb_audio_delegate audio_delegate={
    .rate=44100,
    .chanc=2,
    .cb_pcm_out=cb_pcm_out,
  };
  struct rb_audio *audio=rb_audio_new(rb_audio_type_by_index(0),&audio_delegate);
  RB_ASSERT(audio)
  
  struct rb_synth *synth=rb_synth_new(audio->delegate.rate,audio->delegate.chanc);
  RB_ASSERT(synth)
  audio->delegate.userdata=synth;
  
  RB_ASSERT_CALL(rb_audio_lock(audio))
  RB_ASSERT_CALL(rb_archive_read("out/data",cb_archive,synth))
  RB_ASSERT_CALL(rb_synth_play_note(synth,0x7f,0x37))
  
  RB_ASSERT_INTS(synth->pcmrunc,1)
  checkpcm(synth->pcmrunv[0].pcm);
  
  RB_ASSERT_CALL(rb_audio_unlock(audio))
  
  int repc=100;
  while (repc-->0) {
    if (!synth->pcmrunc) break;
    usleep(10000);
  }
  
  rb_audio_del(audio);
  rb_synth_del(synth);
  return 0;
}
 
XXX_RB_ITEST(defect_20210503_different_sounds,regression) {
  int i=20; while (i-->0) {
    RB_ASSERT_CALL(play_sound())
    usleep(100000);
  }
  return 0;
}
