/* rb_program_store.h
 * Holds the complete set of up to 128 synthesizer programs.
 */
 
#ifndef RB_PROGRAM_STORE_H
#define RB_PROGRAM_STORE_H

struct rb_synth_node_config;

struct rb_program_store {
  struct rb_synth *synth; // WEAK
  int refc;
  struct rb_program_entry {
    struct rb_synth_node_config *config;
    void *serial;
    int serialc;
  } entryv[128];
};

struct rb_program_store *rb_program_store_new(struct rb_synth *synth);
void rb_program_store_del(struct rb_program_store *store);
int rb_program_store_ref(struct rb_program_store *store);

/* Add serial data, or drop everything.
 * Data is any number of:
 *   u8 programid
 *   vlq length
 *   ... node config
 * (programid>=0x80) are reserved for future use and will be ignored here.
 */
int rb_program_store_configure(struct rb_program_store *store,const void *src,int srcc);
int rb_program_store_load_program(struct rb_program_store *store,uint8_t programid,const void *src,int srcc);
int rb_program_store_unconfigure(struct rb_program_store *store);

/* Drop any live objects which might depend on the global output rate.
 * (Call this during a rate change).
 */
int rb_program_store_unload(struct rb_program_store *store);

/* Find PCM for the given note or arrange to generate it.
 * This handles all negotiation with the PCM store too.
 * You must supply a return vector (pcm).
 * (pcmprint) is optional -- if null, we will only return fully printed PCMs (possibly printing the whole thing right now).
 * This can succeed without populating (pcm). That means things are normal but there's no sound this note.
 * Both return vectors are STRONG.
 */
int rb_program_store_get_note(
  struct rb_pcm **pcm,
  struct rb_pcmprint **pcmprint,
  struct rb_program_store *store,
  uint8_t programid,uint8_t noteid
);

/* Read content from the store.
 * Asked for a config, we may decode it if we haven't done yet, and (decode) nonzero.
 */
int rb_program_store_get_serial(
  void *dstpp_WEAK,
  struct rb_program_store *store,
  uint8_t programid
);
struct rb_synth_node_config *rb_program_store_get_config(
  struct rb_program_store *store,
  uint8_t programid,
  int decode
);

#endif
