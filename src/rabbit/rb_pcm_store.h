/* rb_pcm_store.h
 * Cache of printed PCM dumps.
 */
 
#ifndef RB_PCM_STORE_H
#define RB_PCM_STORE_H

struct rb_pcm_store {
  struct rb_synth *synth; // WEAK
  int refc;
  
  // (limit) are the threshold where evictions will happen.
  // (target) are the level we evict to, normally half of (limit).
  int size_limit;
  int size_target;
  int size;
  int count_limit;
  int count_target;
  
  struct rb_pcm_entry {
    uint16_t key;
    struct rb_pcm *pcm;
  } *entryv;
  int entryc,entrya;
};

struct rb_pcm_store *rb_pcm_store_new(struct rb_synth *synth);
void rb_pcm_store_del(struct rb_pcm_store *store);
int rb_pcm_store_ref(struct rb_pcm_store *store);

/* Drop any live objects which might depend on the global output rate.
 * (Call this during a rate change).
 * Or drop just those associated with a given programid, eg if you're changing that program.
 */
int rb_pcm_store_unload(struct rb_pcm_store *store);
int rb_pcm_store_drop_program(struct rb_pcm_store *store,uint8_t programid);

static inline uint16_t rb_pcm_store_generate_key(uint8_t programid,uint8_t noteid) {
  return (programid<<7)|noteid;
}

/* Consumers in general should stick to these two functions.
 * 'get' returns weak, or null if not found.
 * 'add' is not guaranteed to actually store the pcm.
 * You can add a pcm to replace an existing one (but i think that's not the general design).
 */
struct rb_pcm *rb_pcm_store_get(struct rb_pcm_store *store,uint16_t key);
int rb_pcm_store_add(struct rb_pcm_store *store,uint16_t key,struct rb_pcm *pcm);

/* Direct cache access, probably only useful internally.
 * These update all internal state but will not trigger eviction. (only 'add' does that)
 */
int rb_pcm_store_search(const struct rb_pcm_store *store,uint16_t key);
int rb_pcm_store_insert(struct rb_pcm_store *store,int p,uint16_t key,struct rb_pcm *pcm);
int rb_pcm_store_remove(struct rb_pcm_store *store,int p,int c);

/* Main synth will call this when it finishes printing a PCM, to have it persisted to disk.
 */
int rb_pcm_store_persist(struct rb_pcm_store *store,uint16_t key,struct rb_pcm *pcm);

#endif
