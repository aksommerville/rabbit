/* rb_audio.h
 * Provides a callback-driven PCM Out stream.
 */
 
#ifndef RB_AUDIO_H
#define RB_AUDIO_H

struct rb_audio;
struct rb_audio_type;
struct rb_audio_delegate;

/* Audio driver instance.
 *********************************************************/
 
struct rb_audio_delegate {
  void *userdata;
  int rate; // Output rate in Hz -- driver may change during init.
  int chanc; // Channel count, 1 or 2 -- driver may change during init.
  const char *device;
  int (*cb_pcm_out)(int16_t *v,int c,struct rb_audio *audio);
};
 
struct rb_audio {
  const struct rb_audio_type *type;
  int refc;
  struct rb_audio_delegate delegate;
};

/* Beware that drivers may (should!) run a background thread.
 * Your cb_pcm_out may be called before this one completes.
 */
struct rb_audio *rb_audio_new(
  const struct rb_audio_type *type,
  const struct rb_audio_delegate *delegate
);

void rb_audio_del(struct rb_audio *audio);
int rb_audio_ref(struct rb_audio *audio);

/* Implementations probably use only one of update or lock/unlock.
 * It is safe to call these wrapper functions whether implemented or not.
 * Driver guarantees that your callback is not running while you hold the lock.
 */
int rb_audio_update(struct rb_audio *audio);
int rb_audio_lock(struct rb_audio *audio);
int rb_audio_unlock(struct rb_audio *audio);

/* Audio driver type.
 *************************************************************/
 
struct rb_audio_type {
  const char *name;
  const char *desc;
  int objlen;
  void *singleton;
  void (*del)(struct rb_audio *audio);
  int (*init)(struct rb_audio *audio);
  int (*update)(struct rb_audio *audio);
  int (*lock)(struct rb_audio *audio);
  int (*unlock)(struct rb_audio *audio);
};

const struct rb_audio_type *rb_audio_type_by_name(const char *name,int namec);
const struct rb_audio_type *rb_audio_type_by_index(int p);

int rb_audio_type_validate(const struct rb_audio_type *type);

#endif
