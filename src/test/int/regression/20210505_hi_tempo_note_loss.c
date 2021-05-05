#include "test/rb_test.h"
#include <rabbit/rb_audio.h>
#include <rabbit/rb_synth.h>
#include <rabbit/rb_synth_event.h>
#include <rabbit/rb_pcm_store.h>
#include <rabbit/rb_pcm.h>
#include <rabbit/rb_fs.h>
#include <rabbit/rb_archive.h>
#include <unistd.h>

struct context {
  int notec;
};

static int cb_play_note(struct rb_synth *synth,uint8_t programid,uint8_t noteid) {
  struct context *context=synth->userdata;
  context->notec++;
  return 0;
}
 
RB_ITEST(defect_20210505_hi_tempo_note_loss,regression) {

  struct context context={0};
  
  struct rb_synth *synth=rb_synth_new(44100,2);
  RB_ASSERT(synth)
  synth->userdata=&context;
  synth->cb_play_note=cb_play_note;
  
  /* 2021-05-05: I've noticed playing Chetyorska that as the tempo gets faster, notes seem to drop out.
   * Note that fast tempo means multiplier<1.
   * "../chetyorska/src/data/Mvt3_Scherzo.mid" 6133, ok at all tempo
   * "../chetyorska/src/data/nobm.mid" 9313, ok at all tempo
   * "../chetyorska/src/data/anitrasdance.mid" 1561, ok at all tempo
   * OK whatever broke here is either very subtle, or in my imagination.
   * On further review, yeah, I think it was all in my head.
   */
  void *serial=0;
  int serialc=rb_file_read(&serial,"../chetyorska/src/data/anitrasdance.mid");
  RB_ASSERT_CALL(serialc)
  struct rb_song *song=rb_song_new(serial,serialc,synth->rate);
  free(serial);
  RB_ASSERT(song)
  RB_ASSERT_CALL(rb_synth_play_song(synth,song,1))
  rb_song_del(song);
  
  synth->song->repeat=0;
  synth->song->tempomultiplier=0.5;
  
  int panic=1000000;
  while (1) {
    int16_t tmp[1024];
    RB_ASSERT_CALL(rb_synth_update(tmp,1024,synth))
    if (!synth->song) break;
    panic--;
    RB_ASSERT_INTS_OP(panic,>,0)
  }
  
  RB_ASSERT_INTS(context.notec,1561)
  
  rb_synth_del(synth);
  return 0;
}
