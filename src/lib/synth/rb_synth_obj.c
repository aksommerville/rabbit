#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth.h"
#include "rabbit/rb_synth_event.h"
#include "rabbit/rb_pcm.h"
#include "rabbit/rb_program_store.h"
#include "rabbit/rb_pcm_store.h"
#include <stdarg.h>

#define RB_SYNTH_RATE_MIN 100
#define RB_SYNTH_RATE_MAX 200000
#define RB_SYNTH_CHANC_MIN 1
#define RB_SYNTH_CHANC_MAX 8

/* New.
 */
 
struct rb_synth *rb_synth_new(int rate,int chanc) {
  if ((rate<RB_SYNTH_RATE_MIN)||(rate>RB_SYNTH_RATE_MAX)) return 0;
  if ((chanc<RB_SYNTH_CHANC_MIN)||(chanc>RB_SYNTH_CHANC_MAX)) return 0;
  
  struct rb_synth *synth=calloc(1,sizeof(struct rb_synth));
  if (!synth) return 0;
  
  synth->refc=1;
  synth->rate=rate;
  synth->chanc=chanc;
  
  if (
    !(synth->program_store=rb_program_store_new(synth))||
    !(synth->pcm_store=rb_pcm_store_new(synth))
  ) {
    rb_synth_del(synth);
    return 0;
  }
  
  return synth;
}

/* Delete.
 */

void rb_synth_del(struct rb_synth *synth) {
  if (!synth) return;
  if (synth->refc-->1) return;
  
  if (synth->pcmprintv) {
    while (synth->pcmprintc-->0) {
      rb_pcmprint_del(synth->pcmprintv[synth->pcmprintc]);
    }
    free(synth->pcmprintv);
  }
  if (synth->pcmrunv) {
    while (synth->pcmrunc-->0) {
      rb_pcmrun_cleanup(synth->pcmrunv+synth->pcmrunc);
    }
    free(synth->pcmrunv);
  }
  rb_song_player_del(synth->song);
  rb_program_store_del(synth->program_store);
  rb_pcm_store_del(synth->pcm_store);
  if (synth->message) free(synth->message);
  
  free(synth);
}

/* Retain.
 */
 
int rb_synth_ref(struct rb_synth *synth) {
  if (!synth) return -1;
  if (synth->refc<1) return -1;
  if (synth->refc==INT_MAX) return -1;
  synth->refc++;
  return 0;
}

/* Change rate or channel count.
 */

int rb_synth_reinit(struct rb_synth *synth,int rate,int chanc) {
  if ((rate>0)&&(rate!=synth->rate)) {
    if ((rate<RB_SYNTH_RATE_MIN)||(rate>RB_SYNTH_RATE_MAX)) return -1;
    synth->rate=rate;
    rb_synth_silence(synth);
    rb_synth_play_song(synth,0,0);
    while (synth->pcmprintc>0) {
      synth->pcmprintc--;
      rb_pcmprint_del(synth->pcmprintv[synth->pcmprintc]);
    }
    rb_program_store_unload(synth->program_store);
    rb_pcm_store_unload(synth->pcm_store);
  }
  if ((chanc>0)&&(chanc!=synth->chanc)) {
    if ((chanc<RB_SYNTH_CHANC_MIN)||(chanc>RB_SYNTH_CHANC_MAX)) return -1;
    synth->chanc=chanc;
  }
  return 0;
}

/* Load serial data.
 */
 
int rb_synth_configure(struct rb_synth *synth,const void *src,int srcc) {
  if (rb_program_store_configure(synth->program_store,src,srcc)<0) return -1;
  return 0;
}

int rb_synth_load_program(struct rb_synth *synth,uint8_t programid,const void *src,int srcc) {
  if (rb_program_store_load_program(synth->program_store,programid,src,srcc)<0) return -1;
  return 0;
}

int rb_synth_unconfigure(struct rb_synth *synth) {
  if (rb_program_store_unconfigure(synth->program_store)<0) return -1;
  return 0;
}

/* Update PCM printers.
 */
 
static int rb_synth_update_pcmprint(struct rb_synth *synth,int framec) {
  int i=synth->pcmprintc;
  while (i-->0) {
    struct rb_pcmprint *pcmprint=synth->pcmprintv[i];
    int err=rb_pcmprint_update(pcmprint,framec);
    if (err<0) return -1; // Should be rare, and must be serious.
    if (!err) {
      rb_pcmprint_del(pcmprint);
      synth->pcmprintc--;
      memmove(synth->pcmprintv+i,synth->pcmprintv+i+1,sizeof(void*)*(synth->pcmprintc-i));
    }
  }
  return 0;
}

/* Generate signal, mono.
 */
 
static int rb_synth_update_signal_mono(int16_t *v,int c,struct rb_synth *synth) {
  int i=synth->pcmrunc;
  struct rb_pcmrun *pcmrun=synth->pcmrunv+i;
  while (i-->0) {
    pcmrun--;
    if (rb_pcmrun_update(v,c,pcmrun)<=0) {
      rb_pcmrun_cleanup(pcmrun);
      synth->pcmrunc--;
      memmove(pcmrun,pcmrun+1,sizeof(struct rb_pcmrun)*(synth->pcmrunc-i));
    }
  }
  return 0;
}

/* Generate signal, multichannel.
 */
 
static int rb_synth_update_signal_multi(int16_t *v,int c,int framec,struct rb_synth *synth) {
  int i=synth->pcmrunc;
  struct rb_pcmrun *pcmrun=synth->pcmrunv+i;
  while (i-->0) {
    pcmrun--;
    int16_t *vv=v;
    int ii=framec;
    while (ii-->0) {
      if (pcmrun->p>=pcmrun->pcm->c) break;
      int16_t sample=pcmrun->pcm->v[pcmrun->p++];
      int jj=synth->chanc;
      while (jj-->0) {
        (*vv)+=sample;
        vv++;
      }
    }
    if (pcmrun->p>=pcmrun->pcm->c) {
      rb_pcmrun_cleanup(pcmrun);
      synth->pcmrunc--;
      memmove(pcmrun,pcmrun+1,sizeof(struct rb_pcmrun)*(synth->pcmrunc-i));
    }
  }
  return 0;
}

/* Update.
 */
 
int rb_synth_update(int16_t *v,int c,struct rb_synth *synth) {

  int framec=c/synth->chanc;
  if (rb_synth_update_pcmprint(synth,framec)<0) return -1;
  synth->new_printer_framec=framec;

  memset(v,0,c<<1);
  
  if (synth->song) {
    while (framec>0) {
      int err=rb_song_player_update(synth->song);
      if (err<=0) {
        if (err<0) rb_synth_error(synth,"Error updating song");
        rb_song_player_del(synth->song);
        synth->song=0;
        err=framec;
      }
      if (err>framec) err=framec;
      framec-=err;
      if (synth->song) {
        if (rb_song_player_advance(synth->song,err)<0) {
          rb_song_player_del(synth->song);
          synth->song=0;
        }
      }
      if (synth->chanc==1) {
        if (rb_synth_update_signal_mono(v,err,synth)<0) return -1;
        v+=err;
      } else {
        int samplec=err*synth->chanc;
        if (rb_synth_update_signal_multi(v,samplec,err,synth)<0) return -1;
        v+=samplec;
      }
    }
    
  } else if (synth->chanc==1) {
    if (rb_synth_update_signal_mono(v,c,synth)<0) return -1;
  } else {
    if (rb_synth_update_signal_multi(v,c,framec,synth)<0) return -1;
  }
  
  synth->new_printer_framec=0;
  return 0;
}

/* Drop all playback.
 */

int rb_synth_silence(struct rb_synth *synth) {
  while (synth->pcmrunc>0) {
    synth->pcmrunc--;
    rb_pcmrun_cleanup(synth->pcmrunv+synth->pcmrunc);
  }
  return 0;
}

/* Begin song.
 */

int rb_synth_play_song(struct rb_synth *synth,struct rb_song *song,int restart) {

  if (!song) {
    if (!synth->song) return 0;
    rb_song_player_del(synth->song);
    synth->song=0;
    return 0;
  }
  
  if (synth->song&&(synth->song->song==song)) {
    if (!restart) return 0;
    if (rb_song_player_restart(synth->song)<0) return -1;
    return 0;
  }
  
  struct rb_song_player *player=rb_song_player_new(synth,song);
  if (!player) return -1;
  if (synth->song) rb_song_player_del(synth->song);
  synth->song=player;

  return 0;
}

/* Get song tempo phase.
 */
 
int rb_synth_get_song_phase(int *p,int *c,struct rb_synth *synth) {
  *p=*c=0;
  if (!synth->song) return 0;
  if (synth->song->song->ticksperqnote<1) return 0;
  *c=synth->song->song->ticksperqnote;
  *p=synth->song->elapsedinput;
  return 1;
}

/* Add PCM printer.
 */

static int rb_synth_add_pcmprint(struct rb_synth *synth,struct rb_pcmprint *pcmprint) {

  // Entirely possible that we already have it... if so, do nothing and report success.
  int i=synth->pcmprintc;
  while (i-->0) {
    if (synth->pcmprintv[i]==pcmprint) return 0;
  }

  if (synth->pcmprintc>=synth->pcmprinta) {
    int na=synth->pcmprinta+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(synth->pcmprintv,sizeof(void*)*na);
    if (!nv) return -1;
    synth->pcmprintv=nv;
    synth->pcmprinta=na;
  }
  
  // If we are mid-update, print at least enough frames to finish the update.
  // Also, lucky, if that happens to complete it, no need to actually add.
  if (synth->new_printer_framec>0) {
    int err=rb_pcmprint_update(pcmprint,synth->new_printer_framec);
    if (err<=0) return err;
  }
  
  if (rb_pcmprint_ref(pcmprint)<0) return -1;
  synth->pcmprintv[synth->pcmprintc++]=pcmprint;
  
  return 0;
}

/* Add PCM player.
 */
 
static int rb_synth_add_pcm(struct rb_synth *synth,struct rb_pcm *pcm) {

  // TODO Check if already playing within some threshold (eg 50 ms), to protect against haywire notes.

  if (synth->pcmrunc>=synth->pcmruna) {
    int na=synth->pcmruna+16;
    if (na>INT_MAX/sizeof(struct rb_pcmrun)) return -1;
    void *nv=realloc(synth->pcmrunv,sizeof(struct rb_pcmrun)*na);
    if (!nv) return -1;
    synth->pcmrunv=nv;
    synth->pcmruna=na;
  }
  if (rb_pcmrun_init(synth->pcmrunv+synth->pcmrunc,pcm)<0) return -1;
  synth->pcmrunc++;
  return 0;
}

/* Begin note.
 */

int rb_synth_play_note(struct rb_synth *synth,uint8_t programid,uint8_t noteid) {
  
  if (synth->cb_play_note) {
    int err=synth->cb_play_note(synth,programid,noteid);
    if (err<=0) return err;
  }
  
  struct rb_pcm *pcm=0;
  struct rb_pcmprint *pcmprint=0;
  if (rb_program_store_get_note(&pcm,&pcmprint,synth->program_store,programid,noteid)<0) {
    return rb_synth_error(synth,"Failed to acquire PCM for note %02x:%02x",programid,noteid);
  }
  if (pcmprint) {
    int err=rb_synth_add_pcmprint(synth,pcmprint);
    rb_pcmprint_del(pcmprint);
    if (err<0) {
      rb_pcm_del(pcm);
      return -1;
    }
  }
  if (pcm) {
    if (rb_synth_add_pcm(synth,pcm)<0) {
      rb_pcm_del(pcm);
      return -1;
    }
    rb_pcm_del(pcm);
  }
  return 0;
}

/* Receive event.
 */

int rb_synth_event(struct rb_synth *synth,const struct rb_synth_event *event) {
  switch (event->opcode) {
    case RB_SYNTH_EVENT_NOTE_ON: return rb_synth_play_note(synth,synth->chanv[event->chid&0x0f],event->a);
    case RB_SYNTH_EVENT_PROGRAM: {
        synth->chanv[event->chid&0x0f]=event->a;
      } return 0;
    case RB_SYNTH_EVENT_ALL_OFF: return rb_synth_silence(synth);
    // Most events are no-op, wow this is brutally simple!
  }
  return 0;
}

int rb_synth_events(struct rb_synth *synth,const void *src,int srcc) {
  const uint8_t *SRC=src;
  int srcp=0,err;
  while (srcp<srcc) {
    struct rb_synth_event event;
    if ((err=rb_synth_event_decode_stream(&event,SRC+srcp,srcc-srcp))<1) {
      return srcp?srcp:-1;
    }
    srcp+=err;
    if (rb_synth_event(synth,&event)<0) {
      return -1;
    }
  }
  return srcp;
}

/* Sticky error message.
 */
 
int rb_synth_error(struct rb_synth *synth,const char *fmt,...) {
  if (!synth->messagec) {
    if (fmt&&fmt[0]) {
      char tmp[256];
      va_list vargs;
      va_start(vargs,fmt);
      int tmpc=vsnprintf(tmp,sizeof(tmp),fmt,vargs);
      if ((tmpc>0)&&(tmpc<sizeof(tmp))) {
        char *nv=malloc(tmpc+1);
        if (nv) {
          memcpy(nv,tmp,tmpc+1);
          if (synth->message) free(synth->message);
          synth->message=nv;
          synth->messagec=tmpc;
        }
      }
    }
  }
  return -1;
}

void rb_synth_clear_error(struct rb_synth *synth) {
  if (synth->message) free(synth->message);
  synth->message=0;
  synth->messagec=0;
}
