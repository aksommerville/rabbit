#include "test/rb_test.h"
#include <rabbit/rb_audio.h>
#include <rabbit/rb_synth.h>
#include <rabbit/rb_synth_event.h>
#include <rabbit/rb_pcm_store.h>
#include <rabbit/rb_pcm.h>
#include <rabbit/rb_fs.h>
#include <rabbit/rb_archive.h>
#include <unistd.h>

static int cb_pcm(int16_t *v,int c,struct rb_audio *audio) {
  struct rb_synth *synth=audio->delegate.userdata;
  if (synth) {
    if (rb_synth_update(v,c,synth)<0) return -1;
  } else {
    memset(v,0,c<<1);
  }
  return 0;
}

static int cb_archive(uint32_t type,int id,const void *src,int srcc,void *userdata) {
  struct rb_synth *synth=userdata;
  switch (type) {
    case RB_RES_TYPE_snth: {
        //fprintf(stderr,"%02x load program\n",id);
        if (rb_synth_load_program(synth,id,src,srcc)<0) return -1;
      } break;
  }
  return 0;
}

static int playsound(struct rb_audio *audio,struct rb_synth *synth,int id) {
  if (audio) { RB_ASSERT_CALL(rb_audio_lock(audio)) }
  RB_ASSERT_CALL(rb_synth_play_note(synth,id>>8,id&0xff))
  if (audio) { RB_ASSERT_CALL(rb_audio_unlock(audio)) }
  return 0;
}

/* Got some clipping in Chetyorska that made no sense.
 * Culprit turned out to be _rb_osc_update_triangle_ratev(), was going way OOB when rate goes negative.
 */
RB_ITEST(defect_20210502_mystery_clipping,regression) {

  int listen=0;

  struct rb_audio_delegate audio_delegate={
    .rate=44100,
    .chanc=2,
    .cb_pcm_out=cb_pcm,
  };
  struct rb_audio *audio=0;
  if (listen) {
    audio=rb_audio_new(rb_audio_type_by_index(0),&audio_delegate);
    RB_ASSERT(audio);
  }
  
  struct rb_synth *synth=0;
  if (listen) {
    synth=rb_synth_new(audio->delegate.rate,audio->delegate.chanc);
  } else {
    synth=rb_synth_new(audio_delegate.rate,audio_delegate.chanc);
  }
  RB_ASSERT(synth);
  if (listen) audio->delegate.userdata=synth;
  
  if (listen) { RB_ASSERT_CALL(rb_audio_lock(audio)) }
  RB_ASSERT_CALL(rb_archive_read("out/data",cb_archive,synth))
  void *serial=0;
  int serialc=rb_file_read(&serial,"../chetyorska/src/data/song/005-dvorak-o96-3.mid");
  RB_ASSERT_CALL(serialc)
  struct rb_song *song=rb_song_from_midi(serial,serialc);
  free(serial);
  RB_ASSERT(song)
  RB_ASSERT_CALL(rb_synth_play_song(synth,song,1))
  rb_song_del(song);
  if (listen) { RB_ASSERT_CALL(rb_audio_unlock(audio)) }
  
  synth->song->repeat=0;
  
  int phase=0;
  while (1) {
  
    if (listen) {
      usleep(10000);
    } else {
      int16_t tmp[1024];
      RB_ASSERT_CALL(rb_synth_update(tmp,1024,synth))
    }
    if (!synth->song) break;
    
    int nextphase=synth->song->elapsedinput/70;
         if ((nextphase>=1)&&(phase<1)) { playsound(audio,synth,0x7f35); phase=1; }
    else if ((nextphase>=2)&&(phase<2)) { playsound(audio,synth,0x7f35); phase=2; }
    else if ((nextphase>=3)&&(phase<3)) { playsound(audio,synth,0x7f35); phase=3; }
    else if ((nextphase>=4)&&(phase<4)) { playsound(audio,synth,0x7f35); phase=4; }
    else if ((nextphase>=5)&&(phase<5)) { playsound(audio,synth,0x7f35); phase=5; }
    else if ((nextphase>=6)&&(phase<6)) { playsound(audio,synth,0x7f35); phase=6; }
    else if ((nextphase>=7)&&(phase<7)) { playsound(audio,synth,0x7f35); phase=7; }
    else if ((nextphase>=8)&&(phase<8)) { playsound(audio,synth,0x7f35); phase=8; }
    else if ((nextphase>=9)&&(phase<9)) { playsound(audio,synth,0x7f35); phase=9; }
    else if ((nextphase>=10)&&(phase<10)) { playsound(audio,synth,0x7f35); phase=10; }
    else if ((nextphase>=11)&&(phase<11)) { playsound(audio,synth,0x7f35); phase=11; }
    else if ((nextphase>=12)&&(phase<12)) { playsound(audio,synth,0x7f35); phase=12; }
    else if ((nextphase>=13)&&(phase<13)) { playsound(audio,synth,0x7f35); phase=13; }
    else if ((nextphase>=14)&&(phase<14)) { playsound(audio,synth,0x7f36); phase=14; }
    else if ((nextphase>=15)&&(phase<15)) { playsound(audio,synth,0x7f35); phase=15; }
    else if ((nextphase>=16)&&(phase<16)) { playsound(audio,synth,0x7f36); phase=16; }
  }
  
  struct rb_pcm *pcm=rb_pcm_store_get(synth->pcm_store,0x3fb5);
  RB_ASSERT(pcm)
  const int16_t *v=pcm->v;
  int i=pcm->c;
  int16_t lo=v[0],hi=v[1];
  for (;i-->0;v++) {
    if (*v<lo) lo=*v;
    else if (*v>hi) hi=*v;
  }
  //fprintf(stderr,"%d..%d\n",lo,hi);
  // It's a triangle oscillator into env with level=0.15. The peak can't be higher than 0.15.
  int limit=(0x7fff*16)/100;
  RB_ASSERT_INTS_OP(hi,<,limit)
  RB_ASSERT_INTS_OP(lo,>,-limit)
  
  rb_audio_del(audio);
  rb_synth_del(synth);
  return 0;
}
