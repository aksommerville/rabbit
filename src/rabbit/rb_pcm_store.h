/* rb_pcm_store.h
 * Cache of printed PCM dumps.
 */
 
#ifndef RB_PCM_STORE_H
#define RB_PCM_STORE_H

struct rb_pcm_store {
  struct rb_synth *synth; // WEAK
  int refc;
  //TODO
};

struct rb_pcm_store *rb_pcm_store_new(struct rb_synth *synth);
void rb_pcm_store_del(struct rb_pcm_store *store);
int rb_pcm_store_ref(struct rb_pcm_store *store);

/* Drop any live objects which might depend on the global output rate.
 * (Call this during a rate change).
 */
int rb_pcm_store_unload(struct rb_pcm_store *store);

#endif
