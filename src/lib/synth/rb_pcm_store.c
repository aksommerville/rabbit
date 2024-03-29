#include "rabbit/rb_internal.h"
#include "rabbit/rb_pcm_store.h"
#include "rabbit/rb_pcm.h"
#include "rabbit/rb_synth.h"
#include "rabbit/rb_fs.h"

/* New.
 */
 
struct rb_pcm_store *rb_pcm_store_new(struct rb_synth *synth) {
  struct rb_pcm_store *store=calloc(1,sizeof(struct rb_pcm_store));
  if (!store) return 0;
  
  store->synth=synth;
  store->refc=1;
  
  //TODO Once we have realistic test cases, find sensible cache defaults.
  store->size_limit=20*1024*1024;
  store->count_limit=2048;
  store->size_target=store->size_limit>>1;
  store->count_target=store->count_limit>>1;
  
  return store;
}

/* Delete.
 */
 
static void rb_pcm_entry_cleanup(struct rb_pcm_entry *entry) {
  rb_pcm_del(entry->pcm);
}
 
void rb_pcm_store_del(struct rb_pcm_store *store) {
  if (!store) return;
  if (store->refc-->1) return;
  
  if (store->entryv) {
    while (store->entryc-->0) {
      rb_pcm_entry_cleanup(store->entryv+store->entryc);
    }
    free(store->entryv);
  }
  
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
  while (store->entryc>0) {
    store->entryc--;
    rb_pcm_entry_cleanup(store->entryv+store->entryc);
  }
  store->size=0;
  return 0;
}

/* Remove PCMs for a given program.
 */
 
int rb_pcm_store_drop_program(struct rb_pcm_store *store,uint8_t programid) {
  if (programid>=0x80) return 0;
  uint16_t lokey=rb_pcm_store_generate_key(programid,0x00);
  uint16_t hikey=rb_pcm_store_generate_key(programid+1,0x00);
  int lop=rb_pcm_store_search(store,lokey);
  if (lop<0) lop=-lop-1;
  if ((lop>=store->entryc)||(store->entryv[lop].key>=hikey)) return 0; // got none
  int hip=rb_pcm_store_search(store,hikey);
  if (hip<0) hip=-hip-1;
  int c=hip-lop;
  return rb_pcm_store_remove(store,lop,c);
}

/* Consider evicting PCMs.
 */
 
static int rb_pcm_store_check_eviction(struct rb_pcm_store *store) {

  // Within both limits? Cool, do nothing.
  if (
    (store->size<=store->size_limit)&&
    (store->entryc<=store->count_limit)
  ) return 0;
  
  // Drop from the end until both targets are met.
  int c0=store->entryc;
  struct rb_pcm_entry *entry=store->entryv+c0;
  while (
    (store->size>store->size_target)||
    (store->entryc>store->count_target)
  ) {
    store->entryc--;
    entry--;
    store->size-=entry->pcm->c<<1;
    rb_pcm_entry_cleanup(entry);
  }
  
  int rmc=c0-store->entryc;
  fprintf(stderr,
    "rb_pcm_store evicted %d entries. Now count=%d size=%d\n",
    rmc,store->entryc,store->size
  );
  return 0;
}

/* Write to the persistent cache if applicable.
 */
 
int rb_pcm_store_persist(struct rb_pcm_store *store,uint16_t key,struct rb_pcm *pcm) {
  if (!key||!pcm||!pcm->c) return 0;
  const char *root=store->synth->cachedir;
  if (!root||!root[0]) return 0;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/%d/%d",root,store->synth->rate,key);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  rb_mkdir_for_file(path);
  if (rb_file_write(path,pcm->v,pcm->c<<1)<0) return -1;
  //fprintf(stderr,"%s: persisted pcm\n",path);
  return 0;
}

/* If a persistent cache is in play, look for one PCM there.
 * If found, read it, add to the local cache, and return a WEAK reference.
 */
 
static struct rb_pcm *rb_pcm_store_check_persistent_cache(struct rb_pcm_store *store,uint16_t key) {
  const char *root=store->synth->cachedir;
  if (!root||!root[0]) return 0;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/%d/%d",root,store->synth->rate,key);
  if ((pathc<1)||(pathc>=sizeof(path))) return 0;
  void *serial=0;
  int serialc=rb_file_read(&serial,path);
  if (!serial||(serialc<0)||(serialc&1)) return 0;
  
  struct rb_pcm *pcm=rb_pcm_new(serialc>>1);
  if (!pcm) {
    free(serial);
    return 0;
  }
  memcpy(pcm->v,serial,serialc);
  
  int p=rb_pcm_store_search(store,key);
  if (p>=0) { // the hell?
    rb_pcm_del(pcm);
    return 0;
  }
  p=-p-1;
  int err=rb_pcm_store_insert(store,p,key,pcm);
  rb_pcm_del(pcm);
  if (err<0) return 0;
  
  //fprintf(stderr,"%s: got pcm from persistent cache\n",path);
  return pcm;
}

/* Get PCM from cache.
 */
 
struct rb_pcm *rb_pcm_store_get(struct rb_pcm_store *store,uint16_t key) {
  int p=rb_pcm_store_search(store,key);
  if (p<0) return rb_pcm_store_check_persistent_cache(store,key);
  return store->entryv[p].pcm;
}

/* Add PCM to cache.
 */
 
int rb_pcm_store_add(struct rb_pcm_store *store,uint16_t key,struct rb_pcm *pcm) {
  if (!pcm) return -1;
  
  // If we already have an entry, either swap its content or do nothing.
  int p=rb_pcm_store_search(store,key);
  if (p>=0) {
    struct rb_pcm_entry *entry=store->entryv+p;
    if (entry->pcm==pcm) return 0;
    if (rb_pcm_ref(pcm)<0) return -1;
    store->size-=entry->pcm->c<<1;
    store->size+=pcm->c<<1;
    rb_pcm_del(entry->pcm);
    entry->pcm=pcm;
    return rb_pcm_store_check_eviction(store);
  }
  p=-p-1;
  
  // TODO Could add qualification criteria, eg only cache PCMs with fewer than N samples.
  
  if (rb_pcm_store_insert(store,p,key,pcm)<0) return -1;
  return rb_pcm_store_check_eviction(store);
}

/* Search.
 */

int rb_pcm_store_search(const struct rb_pcm_store *store,uint16_t key) {
  int lo=0,hi=store->entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rb_pcm_entry *entry=store->entryv+ck;
         if (key<entry->key) hi=ck;
    else if (key>entry->key) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert.
 */
 
int rb_pcm_store_insert(struct rb_pcm_store *store,int p,uint16_t key,struct rb_pcm *pcm) {
  if ((p<0)||(p>store->entryc)) return -1;
  if (p&&(key<=store->entryv[p-1].key)) return -1;
  if ((p<store->entryc)&&(key>=store->entryv[p].key)) return -1;
  
  if (store->entryc>=store->entrya) {
    int na=store->entrya+32;
    if (na>INT_MAX/sizeof(struct rb_pcm_entry)) return -1;
    void *nv=realloc(store->entryv,sizeof(struct rb_pcm_entry)*na);
    if (!nv) return -1;
    store->entryv=nv;
    store->entrya=na;
  }
  
  struct rb_pcm_entry *entry=store->entryv+p;
  if (rb_pcm_ref(pcm)<0) return -1;
  memmove(entry+1,entry,sizeof(struct rb_pcm_entry)*(store->entryc-p));
  store->entryc++;
  entry->key=key;
  entry->pcm=pcm;
  store->size+=pcm->c<<1;
  
  return 0;
}

/* Remove.
 */
  
int rb_pcm_store_remove(struct rb_pcm_store *store,int p,int c) {
  if (c<1) return 0;
  if (p<0) return -1;
  if (p>store->entryc-c) return -1;
  
  struct rb_pcm_entry *entry0=store->entryv+p;
  struct rb_pcm_entry *entry=entry0;
  int i=c;
  for (;i-->0;entry++) {
    store->size-=entry->pcm->c<<1;
    rb_pcm_entry_cleanup(entry);
  }
  
  store->entryc-=c;
  memmove(entry0,entry0+c,sizeof(struct rb_pcm_entry)*(store->entryc-p));
  
  return 0;
}
