#include "rabbit/rb_internal.h"
#include "rabbit/rb_synth_event.h"
#include "rabbit/rb_synth.h"

/* New.
 */
 
struct rb_song_player *rb_song_player_new(struct rb_synth *synth,struct rb_song *song) {
  if (!synth||!song) return 0;
  
  struct rb_song_player *player=calloc(1,sizeof(struct rb_song_player));
  if (!player) return 0;
  
  if (rb_song_ref(song)<0) {
    free(player);
    return 0;
  }
  
  player->synth=synth;
  player->song=song;
  player->repeat=1;
  
  return player;
}

/* Delete.
 */
 
void rb_song_player_del(struct rb_song_player *player) {
  if (!player) return;
  if (player->refc-->1) return;
  rb_song_del(player->song);
  free(player);
}

/* Retain.
 */
 
int rb_song_player_ref(struct rb_song_player *player) {
  if (!player) return -1;
  if (player->refc<1) return -1;
  if (player->refc==INT_MAX) return -1;
  player->refc++;
  return 0;
}

/* Hard restart: Reinitialize everything.
 */
 
int rb_song_player_restart(struct rb_song_player *player) {
  player->cmdp=0;
  player->delay=0;
  player->elapsedframes=0;
  return 0;
}

/* Process events at time zero.
 */
 
int rb_song_player_update(struct rb_song_player *player) {
  if (player->delay>0) return player->delay;
  while (1) {
  
    if (player->cmdp>=player->song->cmdc) {
      if (player->repeat) {
        // When looping, always report at least one frame of delay.
        player->cmdp=player->song->repeatp;
        player->elapsedframes=0; //TODO maybe need to support restart points that don't align with qnotes
        return 1;
      }
      return 0;
    }
    
    uint16_t cmd=player->song->cmdv[player->cmdp++];
    switch (cmd&RB_SONG_CMD_TYPE_MASK) {
      case RB_SONG_CMD_DELAY: {
          player->delay=cmd;
          if (player->delay>0) return player->delay;
        } break;
      case RB_SONG_CMD_NOTE: {
          uint8_t programid=(cmd>>7)&0x7f;
          uint8_t noteid=cmd&0x7f;
          if (rb_synth_play_note(player->synth,programid,noteid)<0) return -1;
        } break;
      default: return -1;
    }
  }
}

/* Advance time.
 */
 
int rb_song_player_advance(struct rb_song_player *player,int framec) {
  if (framec<1) return 0;
  player->elapsedframes+=framec;
  if (framec<=player->delay) {
    player->delay-=framec;
    return 0;
  }
  player->delay=0;
  // I guess mathematically speaking, we should deliver or skip events until (framec) depleted?
  // This situation is explicitly undefined.
  return 0;
}
