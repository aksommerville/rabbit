#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_event.h"
#include "rabbit/rb_serial.h"

#define RB_CHUNKID_MThd (('M'<<24)|('T'<<16)|('h'<<8)|'d')
#define RB_CHUNKID_MTrk (('M'<<24)|('T'<<16)|('r'<<8)|'k')

/* It's silly to have more than 17 MTrk chunks.
 * (one for each channel, and an extra control track maybe?)
 * We forbid anything beyond that.
 * ...update: For the classical music I'm downloading from http://www.kunstderfuge.com/tchaikovsky.htm, at least one has >17 MTrk.
 */
#define RB_TRACK_LIMIT 30

struct rb_song_track {
  const uint8_t *v; // null if complete
  int c;
  int p;
  int delay; // in ticks; <0 if not read yet
  uint8_t rstat;
};

/* Grow command list.
 */
 
static int rb_song_cmdv_require(struct rb_song *song,int addc) {
  if (addc<1) return 0;
  if (song->cmdc>INT_MAX-addc) return -1;
  int na=song->cmdc+addc;
  if (na<=song->cmda) return 0;
  if (na<INT_MAX-1024) na=(na+1024)&~1023;
  if (na>INT_MAX/sizeof(uint16_t)) return -1;
  void *nv=realloc(song->cmdv,sizeof(uint16_t)*na);
  if (!nv) return -1;
  song->cmdv=nv;
  song->cmda=na;
  return 0;
}

/* Read delay on any track where it's missing.
 * Return the lowest delay, possibly zero, or <0 for encoding errors.
 * Returns INT_MAX at end of file (that would not be encodable as a VLQ).
 */
 
static int rb_song_tracks_require_delay(struct rb_song_track *track,int trackc) {
  int lodelay=INT_MAX;
  for (;trackc-->0;track++) {
    if (!track->v) continue;
    if (track->p>=track->c) {
      track->v=0;
      continue;
    }
    if (track->delay<0) {
      int err=rb_vlq_decode(&track->delay,track->v+track->p,track->c-track->p);
      if (err<=0) return -1;
      track->p+=err;
    }
    if (track->delay<lodelay) lodelay=track->delay;
  }
  return lodelay;
}

/* Add a delay command in ticks.
 */
 
static int rb_song_add_delay(struct rb_song *song,int tickc) {
  int fullc=tickc>>14;
  int partc=tickc&0x3fff;
  if (rb_song_cmdv_require(song,fullc+1)<0) return -1;
  while (fullc-->0) song->cmdv[song->cmdc++]=RB_SONG_CMD_DELAY|0x3fff;
  if (partc) song->cmdv[song->cmdc++]=RB_SONG_CMD_DELAY|partc;
  return 0;
}

/* Drop delay from all tracks.
 */
 
static int rb_song_tracks_consume_delay(struct rb_song_track *track,int trackc,int dropc) {
  for (;trackc-->0;track++) {
    if (!track->v) continue;
    if (track->delay<dropc) return -1;
    track->delay-=dropc;
  }
  return 0;
}

/* Process one event.
 */
 
static int rb_song_add_event(
  struct rb_song *song,
  const struct rb_synth_event *event,
  struct rb_song_track *track,
  uint8_t *chanv
) {

  // Note On. Pretty much the only thing we can do. :)
  if (event->opcode==0x90) {
    if (event->chid<0x10) {
      if (rb_song_cmdv_require(song,1)<0) return -1;
      song->cmdv[song->cmdc++]=RB_SONG_CMD_NOTE|(chanv[event->chid]<<7)|event->a;
    }
    return 0;
  }

  // Detect repeat point. This is a private thing I've been using for some time.
  if ((event->opcode==0xf0)&&(event->c==9)&&!memcmp(event->v,"BBx:START",9)) {
    if (!song->repeatp) song->repeatp=song->cmdc;
    return 0;
  }
  
  // Program Change, store it in (chanv).
  if (event->opcode==0xc0) {
    if (event->chid<0x10) chanv[event->chid]=event->a&0x7f;
    return 0;
  }
  
  // Set Tempo.
  if ((event->opcode==0xff)&&(event->a==0x51)&&(event->c==3)) {
    int usperqnote=(event->v[0]<<16)|(event->v[1]<<8)|event->v[2];
    int ntempo=usperqnote/song->ticksperqnote;
    if (ntempo<1) ntempo=1;
    if (!song->uspertick) {
      song->uspertick=ntempo;
    } else if (ntempo!=song->uspertick) {
      // Song changes tempo -- we choose not to support that.
      fprintf(stderr,"ERROR! Tempo changes not supported (%d => %d)\n",song->uspertick,ntempo);
      return -1;
    }
    return 0;
  }
  
  // ...i guess that's everything.
  return 0;
}

/* Add commands for any events at time zero.
 * Ugly hack: If we receive a tempo change, return it as int (usperqnote).
 */
 
static int rb_song_decode_time_zero(
  struct rb_song *song,
  struct rb_song_track *track,int trackc,
  uint8_t *chanv
) {
  for (;trackc-->0;track++) {
    if (!track->v) continue;
    if (track->delay) continue;
    
    struct rb_synth_event event={0};
    int err=rb_synth_event_decode_file(&event,track->v+track->p,track->c-track->p,&track->rstat);
    if (err<=0) return -1; // malformed event
    track->p+=err;
    track->delay=-1;
    if (rb_song_add_event(song,&event,track,chanv)<0) return -1;
    
    // We could leave it at that, but detecting back-to-back zero-time events is easy, so why not.
    while ((track->p<track->c)&&!track->v[track->p]) {
      track->p++;
      if ((err=rb_synth_event_decode_file(&event,track->v+track->p,track->c-track->p,&track->rstat))<=0) return -1;
      track->p+=err;
      if (rb_song_add_event(song,&event,track,chanv)<0) return -1;
    }
  }
  return 0;
}

/* Flatten tracks into a single command stream.
 */
 
static int rb_song_decode_tracks(
  struct rb_song *song,
  struct rb_song_track *trackv,int trackc
) {
  uint8_t chanv[16]={0}; // programid by chid
  while (1) {
    int lodelay=rb_song_tracks_require_delay(trackv,trackc);
    if (lodelay<0) return -1;
    if (lodelay==INT_MAX) break;
    if (lodelay) {
      if (rb_song_add_delay(song,lodelay)<0) return -1;
      if (rb_song_tracks_consume_delay(trackv,trackc,lodelay)<0) return -1;
    }
    int err=rb_song_decode_time_zero(song,trackv,trackc,chanv);
    if (err<0) return -1;
  }
  
  // If tempo was unset, assume qnote=250ms
  if (!song->uspertick) {
    song->uspertick=250000/song->ticksperqnote;
    if (song->uspertick<1) song->uspertick=1; // not actually possible, i think
  }
  
  return 0;
}

/* Dechunk input and pass forward with a track list.
 */
 
static int rb_song_decode_midi(struct rb_song *song,const uint8_t *src,int srcc) {

  // Read the MThd and all MTrk chunks.
  struct rb_song_track trackv[RB_TRACK_LIMIT]={0};
  int trackc=0;
  int division=0;
  int srcp=0;
  int stopp=srcc-8;
  while (srcp<=stopp) {
    const uint8_t *b=src+srcp;
    int chunkid=(b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];
    int len=(b[4]<<24)|(b[5]<<16)|(b[6]<<8)|b[7];
    srcp+=8;
    b+=8;
    if ((len<0)||(srcp>srcc-len)) return -1;
    srcp+=len;
    switch (chunkid) {
      case RB_CHUNKID_MThd: {
          if (len<6) return -1; // Short MThd
          if (division) return -1; // Multiple MThd
          if (b[0]||(b[1]>1)) return -1; // Format 0 or 1 only
          division=(b[4]<<8)|b[5];
          if (!division||(division&0x8000)) return -1; // No SMPTE timing
        } break;
      case RB_CHUNKID_MTrk: {
          if (trackc>=RB_TRACK_LIMIT) return -1; // Too many tracks
          struct rb_song_track *track=trackv+trackc++;
          track->v=b;
          track->c=len;
          track->p=0;
          track->delay=-1;
          track->rstat=0;
        } break;
      default: break; // Unknown chunk, that's ok
    }
  }
  if (!division) return -1; // No MThd
  if (!trackc) return -1; // No MTrk
  
  song->ticksperqnote=division;
  
  return rb_song_decode_tracks(song,trackv,trackc);
}

/* New, MIDI.
 */
 
struct rb_song *rb_song_from_midi(const void *src,int srcc) {
  if (!src||(srcc<0)) return 0;
  struct rb_song *song=calloc(1,sizeof(struct rb_song));
  if (!song) return 0;
  
  song->refc=1;
  
  if (rb_song_decode_midi(song,src,srcc)<0) {
    rb_song_del(song);
    return 0;
  }
  
  return song;
}

/* New, from our format.
 */
 
struct rb_song *rb_song_new(const void *src,int srcc) {
  if (!src||(srcc<16)||memcmp(src,"r\xabSg",4)) return 0;
  struct rb_song *song=calloc(1,sizeof(struct rb_song));
  if (!song) return 0;
  
  song->refc=1;
  
  const uint8_t *SRC=src;
  if (memcmp(SRC+8,"\0\0\0\0\0\0\0\0",8)) { // reserved, must be zero
    rb_song_del(song);
    return 0;
  }
  
  song->uspertick=(SRC[4]<<8)|SRC[5];
  song->ticksperqnote=(SRC[6]<<8)|SRC[7];
  if (!song->uspertick||!song->ticksperqnote) {
    rb_song_del(song);
    return 0;
  }
  
  int srcp=16;
  int cmdc=(srcc-srcp)/2;
  if (rb_song_cmdv_require(song,cmdc)<0) {
    rb_song_del(song);
    return 0;
  }
  
  // Commands are already in the format we want. Just correct the byte order and ensure no reserved commands are used.
  #if BYTE_ORDER==LITTLE_ENDIAN
    {
      uint8_t *dstbytes=(uint8_t*)(song->cmdv);
      const uint8_t *srcbytes=SRC+srcp;
      int i=cmdc;
      for (;i-->0;dstbytes+=2,srcbytes+=2) {
        if (srcbytes[0]&0x40) {
          rb_song_del(song);
          return 0;
        }
        dstbytes[0]=srcbytes[1];
        dstbytes[1]=srcbytes[0];
      }
    }
  #else
    memcpy(song->cmdv,SRC+srcp,cmdc<<1);
    const uint16_t *cmd=song->cmdv;
    int i=cmdc; for (;i-->0;cmd++) {
      if ((*cmd)&0x4000) {
        rb_song_del(song);
        return 0;
      }
    }
  #endif
  song->cmdc=cmdc;
  
  return song;
}

/* Delete.
 */
 
void rb_song_del(struct rb_song *song) {
  if (!song) return;
  if (song->refc-->1) return;
  if (song->cmdv) free(song->cmdv);
  free(song);
}

/* Retain.
 */
 
int rb_song_ref(struct rb_song *song) {
  if (!song) return -1;
  if (song->refc<1) return -1;
  if (song->refc==INT_MAX) return -1;
  song->refc++;
  return 0;
}
