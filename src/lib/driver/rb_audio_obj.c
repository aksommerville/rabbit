#include "rabbit/rb_internal.h"
#include "rabbit/rb_audio.h"
#include "rabbit/rb_image.h"

/* New.
 */
 
struct rb_audio *rb_audio_new(
  const struct rb_audio_type *type,
  const struct rb_audio_delegate *delegate
) {
  if (!type) {
    if (!(type=rb_audio_type_by_index(0))) return 0;
  }
  
  struct rb_audio *audio=0;
  if (type->singleton) {
    audio=type->singleton;
    if (audio->refc) return 0;
  } else {
    if (!(audio=calloc(1,type->objlen))) return 0;
  }
  audio->type=type;
  audio->refc=1;
  
  if (delegate) audio->delegate=*delegate;
  if (audio->delegate.rate<1) audio->delegate.rate=44100;
  if (audio->delegate.chanc<1) audio->delegate.chanc=1;
  
  if (type->init) {
    if (type->init(audio)<0) {
      rb_audio_del(audio);
      return 0;
    }
  }
  
  return audio;
}

/* Delete.
 */

void rb_audio_del(struct rb_audio *audio) {
  if (!audio) return;
  if (audio->refc-->1) return;
  if (audio->type->del) audio->type->del(audio);
  if (audio==audio->type->singleton) memset(audio,0,audio->type->objlen);
  else free(audio);
}

/* Retain.
 */
 
int rb_audio_ref(struct rb_audio *audio) {
  if (!audio) return -1;
  if (audio->refc<1) return -1;
  if (audio->refc==INT_MAX) return -1;
  audio->refc++;
  return 0;
}

/* Hook wrappers.
 */
 
int rb_audio_update(struct rb_audio *audio) {
  if (!audio->type->update) return 0;
  return audio->type->update(audio);
}

int rb_audio_lock(struct rb_audio *audio) {
  if (!audio->type->lock) return 0;
  return audio->type->lock(audio);
}

int rb_audio_unlock(struct rb_audio *audio) {
  if (!audio->type->unlock) return 0;
  return audio->type->unlock(audio);
}
