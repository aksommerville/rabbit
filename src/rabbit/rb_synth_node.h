/* rb_synth_node.h
 * The interesting parts of the synthesizer are generalized into nodes.
 *
 * There are three types:
 *  - type: Static definition of a node type, established at compile time.
 *  - config: Blueprint from which nodes can be instantiated, hews close to the serial format.
 *  - runner: Live node ready to produce a PCM stream.
 *
 * Unlike the outer layers of synth, nodes use floating-point samples.
 */
 
#ifndef RB_SYNTH_NODE_H
#define RB_SYNTH_NODE_H

struct rb_synth_node_type;
struct rb_synth_node_field;
struct rb_synth_node_link;
struct rb_synth_node_config;
struct rb_synth_node_runner;

#include "rabbit/rb_signal.h"

/* Runner.
 ************************************************************/
 
struct rb_synth_node_runner {
  struct rb_synth_node_config *config; // STRONG, REQUIRED
  int refc;
  void (*update)(struct rb_synth_node_runner *runner,int c);
};

/* Caller must create all the necessary buffers and provide them here.
 * These must remain allocated throughout the runner's life.
 */
struct rb_synth_node_runner *rb_synth_node_runner_new(
  struct rb_synth_node_config *config,
  rb_sample_t **bufferv,int bufferc,
  uint8_t noteid
);

void rb_synth_node_runner_del(struct rb_synth_node_runner *runner);
int rb_synth_node_runner_ref(struct rb_synth_node_runner *runner);

/* Get total duration in frames.
 * Not every node can do this, but any that interacts with pcmprint must.
 * That means the high level "program" nodes all must do it.
 */
int rb_synth_node_runner_get_duration(struct rb_synth_node_runner *runner);

/* Config.
 **********************************************************/
 
#define RB_SYNTH_BUFFER_COUNT 0x10
#define RB_SYNTH_LINK_NOTEID  0x10
#define RB_SYNTH_LINK_NOTEHZ  0x11
// For serial field definitions, not saved as links:
#define RB_SYNTH_LINK_S15_16  0x80 /* 4 bytes data */
#define RB_SYNTH_LINK_ZERO    0x81 /* no data */
#define RB_SYNTH_LINK_ONE     0x82 /* no data */
#define RB_SYNTH_LINK_NONE    0x83 /* no data */
#define RB_SYNTH_LINK_SERIAL1 0x84 /* u8 length + data */
#define RB_SYNTH_LINK_SERIAL2 0x85 /* u16 length + data*/
#define RB_SYNTH_LINK_U8      0x86 /* u8 data (int) */
 
struct rb_synth_node_link {
  const struct rb_synth_node_field *field;
  uint8_t bufferid;
};
 
struct rb_synth_node_config {
  const struct rb_synth_node_type *type;
  struct rb_synth *synth; // WEAK
  int refc;
  int ready;
  struct rb_synth_node_link *linkv;
  int linkc,linka;
};

struct rb_synth_node_config *rb_synth_node_config_new(
  struct rb_synth *synth,
  const struct rb_synth_node_type *type
);

void rb_synth_node_config_del(struct rb_synth_node_config *config);
int rb_synth_node_config_ref(struct rb_synth_node_config *config);

/* Convenience to decode and ready a config in one shot.
 * First byte of (src) is ntid, after that generic fields a la rb_synth_node_config_decode_partial().
 */
struct rb_synth_node_config *rb_synth_node_config_new_decode(
  struct rb_synth *synth,
  const void *src,int srcc
);

/* May only call before ready.
 * Read serial data from (src) and assign constants or record links.
 * Returns length consumed.
 * Consists of any number of field definitions:
 *   FLDID BUFFERID ...
 * Optionally terminated with a zero FLDID.
 */
int rb_synth_node_config_decode_partial(
  struct rb_synth_node_config *config,
  const void *src,int srcc
);

int rb_synth_node_config_ready(struct rb_synth_node_config *config);

// Returns bufferid or <0 for the given field.
int rb_synth_node_config_find_link(const struct rb_synth_node_config *config,uint8_t fldid);
int rb_synth_node_config_field_is_buffer(const struct rb_synth_node_config *config,uint8_t fldid);

/* Type.
 ************************************************************/
 
#define RB_SYNTH_NODE_FIELD_REQUIRED     0x0001
 
struct rb_synth_node_field {
  uint8_t fldid;
  const char *name;
  const char *desc;
  int flags;
  const char *enumlabels;
  int config_offseti;
  int config_offsetf;
  int runner_offsetv;
  int runner_offsetf;
  int (*config_sets)(struct rb_synth_node_config *config,const void *src,int srcc);
};
 
struct rb_synth_node_type {
  uint8_t ntid;
  const char *name;
  const char *desc;
  int flags;
  int config_objlen;
  int runner_objlen;
  
  const struct rb_synth_node_field *fieldv;
  int fieldc;
  
  void (*config_del)(struct rb_synth_node_config *config);
  void (*runner_del)(struct rb_synth_node_runner *runner);
  
  int (*config_init)(struct rb_synth_node_config *config);
  int (*config_ready)(struct rb_synth_node_config *config);
  
  /* REQUIRED
   * This must set (runner->update).
   * All runner fields will have been assigned already by the wrapper.
   * (noteid) can be linked generically but is also provided here in case you treat it special.
   */
  int (*runner_init)(struct rb_synth_node_runner *runner,uint8_t noteid);
  
  int (*runner_get_duration)(struct rb_synth_node_runner *runner);
};

const struct rb_synth_node_type *rb_synth_node_type_by_id(uint8_t ntid);
const struct rb_synth_node_type *rb_synth_node_type_by_name(const char *name,int namec);
const struct rb_synth_node_type *rb_synth_node_type_by_index(int p);

int rb_synth_node_type_install(const struct rb_synth_node_type *type,int clobber);
int rb_synth_node_type_validate(const struct rb_synth_node_type *type);

const struct rb_synth_node_field *rb_synth_node_field_by_id(const struct rb_synth_node_type *type,uint8_t fldid);
const struct rb_synth_node_field *rb_synth_node_field_by_name(const struct rb_synth_node_type *type,const char *name,int namec);

#define RB_SYNTH_NTID_noop            0x00 /* TODO */
#define RB_SYNTH_NTID_program         0x01 /* TODO contains buffers and nodes for generic design */
#define RB_SYNTH_NTID_beep            0x02
#define RB_SYNTH_NTID_gain            0x03

extern const struct rb_synth_node_type rb_synth_node_type_beep;
extern const struct rb_synth_node_type rb_synth_node_type_gain;

#endif
