/* rb_synth_event.h
 */
 
#ifndef RB_SYNTH_EVENT_H
#define RB_SYNTH_EVENT_H

// Our synthesizer is very simple, there's only 3 MIDI events we care about.
#define RB_SYNTH_EVENT_NOTE_ON   0x90
#define RB_SYNTH_EVENT_PROGRAM   0xc0
#define RB_SYNTH_EVENT_ALL_OFF   0xff

#define RB_CHID_ALL 0xff

struct rb_synth_event {
  uint8_t opcode; // RB_SYNTH_EVENT_*, normally the high 4 bits of the leading byte.
  uint8_t chid; // 0..15 or RB_CHID_ALL
  uint8_t a,b; // Two verbatim payload bytes, for most events.
  const uint8_t *v; // Arbitrary data for Sysex and Meta (which we don't use)
  int c;
};

/* Both return the length consumed, 0 only for short input.
 * Decoding from a file, you must record Running Status (rstat). Zero initially.
 */
int rb_synth_event_decode_file(struct rb_synth_event *event,const void *src,int srcc,uint8_t *rstat);
int rb_synth_event_decode_stream(struct rb_synth_event *event,const void *src,int srcc);

/* Every song command is 16 bits.
 * The top 2 bits describe it.
 */
#define RB_SONG_CMD_TYPE_MASK   0xc000
#define RB_SONG_CMD_DELAY       0x0000 /* 3fff=delay in frames */
#define RB_SONG_CMD_NOTE        0x8000 /* 3f80=programid, 007f=noteid */
// 4000 and c000 are reserved

struct rb_song {
  int refc;
  int repeatp;
  uint16_t *cmdv;
  int cmdc,cmda;
  int uspertick;
  int ticksperqnote;
};

struct rb_song *rb_song_new(const void *src,int srcc);
struct rb_song *rb_song_from_midi(const void *src,int srcc);

void rb_song_del(struct rb_song *song);
int rb_song_ref(struct rb_song *song);

struct rb_song_player {
  struct rb_synth *synth;
  int refc;
  struct rb_song *song;
  int repeat; // boolean, set directly
  int cmdp;
  int delay; // consume this delay before reading (cmdp), in output frames
  float naturaltempo; // constant; derived from song and context
  float tempoadjust; // Default 1, may vary. Longer is slower. Do not set directly.
  float framespertick; // naturaltempo*tempoadjust
  float ticksperframe; // 1/framespertick
  int elapsedoutput; // Count of frames emitted.
  int elapsedinput; // Count of ticks consumed from the song.
  int elapsedinputnext; // (elapsedinput) snaps to this when delay reaches zero, to mitigate rounding error.
};

struct rb_song_player *rb_song_player_new(struct rb_synth *synth,struct rb_song *song);
void rb_song_player_del(struct rb_song_player *player);
int rb_song_player_ref(struct rb_song_player *player);

/* Deliver any events at time zero, skip past them, return count of frames to next event.
 * Returns zero only at end of song when repeat disabled, or <0 on error.
 * This does not change the player's internal clock.
 */
int rb_song_player_update(struct rb_song_player *player);

/* Advance by so many frames -- less or equal to the last value returned from rb_song_player_update().
 * Behavior is undefined if you advance by more than that.
 */
int rb_song_player_advance(struct rb_song_player *player,int framec);

int rb_song_player_restart(struct rb_song_player *player);

/* Set the absolute tempo adjustment (player->tempoadjust, which is read-only).
 * 1 is the default, play at the natural tempo.
 * 0.5 is twice as fast, 2.0 twice as slow, etc.
 */
int rb_song_player_adjust_tempo(struct rb_song_player *player,float adjust);

#endif
