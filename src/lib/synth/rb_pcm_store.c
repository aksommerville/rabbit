#include "rabbit/rb_internal.h"
#include "rabbit/rb_pcm_store.h"
#include "rabbit/rb_pcm.h"

/* New.
 */
 
struct rb_pcm_store *rb_pcm_store_new(struct rb_synth *synth) {
  struct rb_pcm_store *store=calloc(1,sizeof(struct rb_pcm_store));
  if (!store) return 0;
  
  store->synth=synth;
  store->refc=1;
  
  return store;
}

/* Delete.
 */
 
void rb_pcm_store_del(struct rb_pcm_store *store) {
  if (!store) return;
  if (store->refc-->1) return;
  
  free(store);
}

/* Retain.
 */
 
int rb_pcm_store_ref(struct rb_pcm_store *store) {
  if (!store) return -1;
  if (store->refc<1) return -1;
  if (store->refc==INT_MAX) return -1;
  store->refc++;
  return 0;
}

/* Clear cache.
 */
 
int rb_pcm_store_unload(struct rb_pcm_store *store) {
  //TODO
  return 0;
}
