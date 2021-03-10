#include "rabbit/rb_internal.h"
#include "rabbit/rb_program_store.h"
#include "rabbit/rb_serial.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_pcm.h"

/* New.
 */

struct rb_program_store *rb_program_store_new(struct rb_synth *synth) {
  struct rb_program_store *store=calloc(1,sizeof(struct rb_program_store));
  if (!store) return 0;
  
  store->synth=synth;
  store->refc=1;
  
  return store;
}

/* Delete.
 */
 
static void rb_program_entry_cleanup(struct rb_program_entry *entry) {
  if (entry->config) rb_synth_node_config_del(entry->config);
  if (entry->serial) free(entry->serial);
}
 
void rb_program_store_del(struct rb_program_store *store) {
  if (!store) return;
  if (store->refc-->1) return;
  
  struct rb_program_entry *entry=store->entryv;
  int i=128;
  for (;i-->0;entry++) rb_program_entry_cleanup(entry);
  
  free(store);
}

/* Retain.
 */
 
int rb_program_store_ref(struct rb_program_store *store) {
  if (!store) return -1;
  if (store->refc<1) return -1;
  if (store->refc==INT_MAX) return -1;
  store->refc++;
  return 0;
}

/* Store a serial chunk.
 */
 
static int rb_program_store_receive_serial(struct rb_program_store *store,uint8_t programid,const void *src,int srcc) {
  struct rb_program_entry *entry=store->entryv+programid;
  
  // If we already have identical content, stop.
  // In that case, the config may already exist and it's nice to keep it.
  if ((entry->serialc==srcc)&&!memcmp(entry->serial,src,srcc)) {
    return 0;
  }
  
  void *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (entry->config) {
    rb_synth_node_config_del(entry->config);
    entry->config=0;
  }
  if (entry->serial) free(entry->serial);
  entry->serial=nv;
  entry->serialc=srcc;
  
  return 0;
}

/* Configure.
 */

int rb_program_store_configure(struct rb_program_store *store,const void *src,int srcc) {
  const uint8_t *SRC=src;
  int srcp=0;
  while (srcp<srcc) {
    uint8_t programid=SRC[srcp++];
    int len,err;
    if ((err=rb_vlq_decode(&len,SRC+srcp,srcc-srcp))<1) return -1;
    srcp+=err;
    if (srcp>srcc-len) return -1;
    const void *serial=SRC+srcp;
    srcp+=len;
    if (programid<0x80) {
      if (rb_program_store_receive_serial(store,programid,serial,len)<0) return -1;
    }
  }
  return 0;
}

/* Drop configuration.
 */
 
int rb_program_store_unconfigure(struct rb_program_store *store) {
  struct rb_program_entry *entry=store->entryv;
  int i=128;
  for (;i-->0;entry++) rb_program_entry_cleanup(entry);
  memset(store->entryv,0,sizeof(store->entryv));
  return 0;
}

/* Drop caches.
 */

int rb_program_store_unload(struct rb_program_store *store) {
  struct rb_program_entry *entry=store->entryv;
  int i=128;
  for (;i-->0;entry++) {
    if (entry->config) {
      rb_synth_node_config_del(entry->config);
      entry->config=0;
    }
  }
  return 0;
}

/* Get pcm.
 */

int rb_program_store_get_note(
  struct rb_pcm **pcm,
  struct rb_pcmprint **pcmprint_rtn,
  struct rb_program_store *store,
  uint8_t programid,uint8_t noteid
) {
  if (programid>=0x80) return 0;
  if (noteid>=0x80) return 0;
  
  //TODO check PCM store
  
  // Acquire node config.
  struct rb_program_entry *entry=store->entryv+programid;
  if (!entry->config) {
    if (!entry->serialc) return 0;
    if (!(entry->config=rb_synth_node_config_new_decode(store->synth,entry->serial,entry->serialc))) {
      //TODO report error. we'll drop the serial data so as not to attempt it again in the future.
      // I'm not calling this a hard error, but it definitely deserves loud reporting!
      free(entry->serial);
      entry->serial=0;
      entry->serialc=0;
      return 0;
    }
  }
  
  // Make a PCM printer -- it takes care of instantiating the node.
  struct rb_pcmprint *pcmprint=rb_pcmprint_new(entry->config,noteid);
  if (!pcmprint) return -1;
  
  //TODO add to PCM cache
  
  // If the caller didn't supply a printer return vector, we must run it to completion right now.
  if (!pcmprint_rtn) {
    if (rb_pcmprint_update(pcmprint,pcmprint->pcm->c)<0) {
      rb_pcmprint_del(pcmprint);
      return -1;
    }
    *pcm=pcmprint->pcm; // HANDOFF
    pcmprint->pcm=0;
    rb_pcmprint_del(pcmprint);
    return 0;
  }
  
  // Normal case: Give the PCM and the print to our caller and we're done.
  if (rb_pcm_ref(pcmprint->pcm)<0) {
    rb_pcmprint_del(pcmprint);
    return -1;
  }
  *pcm=pcmprint->pcm;
  *pcmprint_rtn=pcmprint; // HANDOFF
  
  return 0;
}
