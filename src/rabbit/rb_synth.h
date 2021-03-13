/* rb_synth.h
 * Generates the PCM stream you feed to the audio driver.
 * We impose some harsh restrictions:
 *  - Monaural only.
 *  - Integer samples only (signed 16-bit).
 *  - No velocity, sustain, or note expression, everything is fire-and-forget.
 * The samples we play are generated dynamically.
 * So any output rate is OK, within reason.
 * And the second and subsequent times you play a note, it's dirt cheap.
 */
 
#ifndef RB_SYNTH_H
#define RB_SYNTH_H

struct rb_pcmprint;
struct rb_pcmrun;
struct rb_song_player;
struct rb_program_store;
struct rb_pcm_store;

struct rb_synth {
  int refc;
  int rate;
  int chanc; // If >1, all channels will get the same signal.
  
  struct rb_pcmprint **pcmprintv;
  int pcmprintc,pcmprinta;
  struct rb_pcmrun *pcmrunv;
  int pcmrunc,pcmruna;
  struct rb_song_player *song;
  uint8_t chanv[16]; // Program ID by Channel ID
  int new_printer_framec;
  
  struct rb_program_store *program_store;
  struct rb_pcm_store *pcm_store;
  
  char *message;
  int messagec;
};

/* (rate) in Hertz, must match the driver.
 * (chanc) should match the driver unless you want to expand it yourself.
 */
struct rb_synth *rb_synth_new(int rate,int chanc);

void rb_synth_del(struct rb_synth *synth);
int rb_synth_ref(struct rb_synth *synth);

/* Change rate or channel count for a running synth.
 * <=0 to preserve current value.
 * If rate actually changes, this will interrupt playback.
 * Changing rate is detrimental to performance; we have to discard any caches.
 * This does not drop any loaded configuration data.
 */
int rb_synth_reinit(struct rb_synth *synth,int rate,int chanc);

/* Load encoded program configurations.
 * You can "configure" multiple times; old content remains unless overwritten specifically.
 * "unconfigure" to drop all prior config data.
 */
int rb_synth_configure(struct rb_synth *synth,const void *src,int srcc);
int rb_synth_unconfigure(struct rb_synth *synth);

/* (c) in samples regardless of chanc -- not frames, not bytes.
 * It would be disastrous for events to arrive while update is running; you must guard against that.
 */
int rb_synth_update(int16_t *v,int c,struct rb_synth *synth);

/* Drop all sounds cold. There may be a pop.
 */
int rb_synth_silence(struct rb_synth *synth);

/* Begin playing a MIDI file.
 * There is only one song at a time.
 * If you provide null, we stop the current song.
 * If this one is already playing we either do nothing (restart==0) or restart it (restart!=0).
 */
int rb_synth_play_song(struct rb_synth *synth,struct rb_song *song,int restart);

/* Start a note or sound effect.
 * You provide Program ID, not Channel ID -- channels are a very weak concept for this synth.
 */
int rb_synth_play_note(struct rb_synth *synth,uint8_t programid,uint8_t noteid);

/* Trigger events from a MIDI device.
 * These share the Channel space with the song if present.
 */
int rb_synth_event(struct rb_synth *synth,const struct rb_synth_event *event);
int rb_synth_events(struct rb_synth *synth,const void *src,int srcc);

/* Setting error message always returns -1, for convenience.
 * If a message is already present, rb_synth_error() will *not* replace it.
 * Internally, synth ops may set this message but not actually fail, meaning something went wrong but we can proceed without.
 * Owner should check the message after any failure, and periodically otherwise.
 * (no harm if you don't)
 */
int rb_synth_error(struct rb_synth *synth,const char *fmt,...);
void rb_synth_clear_error(struct rb_synth *synth);

#endif
