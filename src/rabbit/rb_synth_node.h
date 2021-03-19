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
 * (bufferv) can be sparse, we check everything as we use it.
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

/* Generic field value.
 ***********************************************************/

// Field types 0..15 are buffer IDs, and 0..127 are dynamically linked.
#define RB_SYNTH_FIELD_TYPE_BUFFER0    0x00
#define RB_SYNTH_FIELD_TYPE_BUFFER1    0x01
#define RB_SYNTH_FIELD_TYPE_BUFFER2    0x02
#define RB_SYNTH_FIELD_TYPE_BUFFER3    0x03
#define RB_SYNTH_FIELD_TYPE_BUFFER4    0x04
#define RB_SYNTH_FIELD_TYPE_BUFFER5    0x05
#define RB_SYNTH_FIELD_TYPE_BUFFER6    0x06
#define RB_SYNTH_FIELD_TYPE_BUFFER7    0x07
#define RB_SYNTH_FIELD_TYPE_BUFFER8    0x08
#define RB_SYNTH_FIELD_TYPE_BUFFER9    0x09
#define RB_SYNTH_FIELD_TYPE_BUFFER10   0x0a
#define RB_SYNTH_FIELD_TYPE_BUFFER11   0x0b
#define RB_SYNTH_FIELD_TYPE_BUFFER12   0x0c
#define RB_SYNTH_FIELD_TYPE_BUFFER13   0x0d
#define RB_SYNTH_FIELD_TYPE_BUFFER14   0x0e
#define RB_SYNTH_FIELD_TYPE_BUFFER15   0x0f
#define RB_SYNTH_BUFFER_COUNT          0x10
#define RB_SYNTH_FIELD_TYPE_NOTEID     0x10
#define RB_SYNTH_FIELD_TYPE_NOTEHZ     0x11
// Field types 128..255 are statically linked, for constants.
#define RB_SYNTH_FIELD_TYPE_S15_16     0x80 /* s15.16 as float */
#define RB_SYNTH_FIELD_TYPE_ZERO       0x81 /* float */
#define RB_SYNTH_FIELD_TYPE_ONE        0x82 /* float */
#define RB_SYNTH_FIELD_TYPE_NONE       0x83 /* float */
#define RB_SYNTH_FIELD_TYPE_SERIAL1    0x84 /* u8 length + data */
#define RB_SYNTH_FIELD_TYPE_SERIAL2    0x85 /* u16 length + data */
#define RB_SYNTH_FIELD_TYPE_U8         0x86 /* u8 as int */
#define RB_SYNTH_FIELD_TYPE_U0_8       0x87 /* u0.8 as float */

// Logical type, ie how should code interpret it.
#define RB_SYNTH_FIELD_LOG_TYPE_LINK      1
#define RB_SYNTH_FIELD_LOG_TYPE_FLOAT     2
#define RB_SYNTH_FIELD_LOG_TYPE_INT       3
#define RB_SYNTH_FIELD_LOG_TYPE_SERIAL    4

#define RB_SYNTH_SERIALFMT_UNSPEC    0
#define RB_SYNTH_SERIALFMT_NODE      1
#define RB_SYNTH_SERIALFMT_NODES     2
#define RB_SYNTH_SERIALFMT_ENV       3
#define RB_SYNTH_SERIALFMT_COEFV     4
#define RB_SYNTH_SERIALFMT_MULTIPLEX 5
#define RB_SYNTH_SERIALFMT_HEXDUMP   6 /* used internally by aucm */

struct rb_synth_field_value {
  uint8_t type; // RB_SYNTH_FIELD_TYPE_*
  uint8_t logtype; // RB_SYNTH_FIELD_LOG_TYPE_*
  rb_sample_t f;
  int i;
  const void *v;
  int c;
};

int rb_synth_field_value_decode(struct rb_synth_field_value *dst,const void *src,int srcc);

/* Config.
 **********************************************************/
 
struct rb_synth_node_link {
  const struct rb_synth_node_field *field;
  uint8_t type; // RB_SYNTH_FIELD_TYPE_*
};
 
struct rb_synth_node_config {
  const struct rb_synth_node_type *type;
  struct rb_synth *synth; // WEAK
  int refc;
  int ready;
  struct rb_synth_node_link *linkv;
  int linkc,linka;
  uint32_t assigned; // (1<<(fldid-1)) for fldid 1..32
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
 * Returns length consumed, including terminator if present.
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
#define RB_SYNTH_NODE_FIELD_BUF0IFNONE   0x0002 /* Assign buffer 0 if unset at ready */
#define RB_SYNTH_NODE_FIELD_PRINCIPAL    0x0004 /* Serial format to accept unkeyed assignment, limit 1 */
 
struct rb_synth_node_field {
  uint8_t fldid;
  const char *name;
  const char *desc;
  int flags;
  int serialfmt;
  const char *enumlabels;
  int config_offseti;
  int config_offsetf;
  int runner_offsetv;
  int runner_offsetf;
  int (*config_sets)(struct rb_synth_node_config *config,const void *src,int srcc);
};

#define RB_SYNTH_NODE_TYPE_PROGRAM    0x0001 /* Suitable for use as a top-level program. */
 
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
const struct rb_synth_node_field *rb_synth_node_principal_field(const struct rb_synth_node_type *type);

#define RB_SYNTH_NTID_noop            0x00
#define RB_SYNTH_NTID_instrument      0x01
#define RB_SYNTH_NTID_beep            0x02
#define RB_SYNTH_NTID_gain            0x03
#define RB_SYNTH_NTID_osc             0x04
#define RB_SYNTH_NTID_env             0x05
#define RB_SYNTH_NTID_add             0x06
#define RB_SYNTH_NTID_mlt             0x07
#define RB_SYNTH_NTID_fm              0x08
#define RB_SYNTH_NTID_harm            0x09
#define RB_SYNTH_NTID_multiplex       0x0a

extern const struct rb_synth_node_type rb_synth_node_type_noop;
extern const struct rb_synth_node_type rb_synth_node_type_instrument;
extern const struct rb_synth_node_type rb_synth_node_type_beep;
extern const struct rb_synth_node_type rb_synth_node_type_gain;
extern const struct rb_synth_node_type rb_synth_node_type_osc;
extern const struct rb_synth_node_type rb_synth_node_type_env;
extern const struct rb_synth_node_type rb_synth_node_type_add;
extern const struct rb_synth_node_type rb_synth_node_type_mlt;
extern const struct rb_synth_node_type rb_synth_node_type_fm;
extern const struct rb_synth_node_type rb_synth_node_type_harm;
extern const struct rb_synth_node_type rb_synth_node_type_multiplex;

/* API for specific node types.
 ****************************************************************/

#define RB_OSC_SHAPE_SINE      0x00
#define RB_OSC_SHAPE_SQUARE    0x01
#define RB_OSC_SHAPE_SAWUP     0x02
#define RB_OSC_SHAPE_SAWDOWN   0x03
#define RB_OSC_SHAPE_TRIANGLE  0x04
#define RB_OSC_SHAPE_IMPULSE   0x05
#define RB_OSC_SHAPE_NOISE     0x06
#define RB_OSC_SHAPE_DC        0x07

#define RB_ENV_FLAG_PRESET       0x80
// If not PRESET:
#define RB_ENV_FLAG_TIME_RANGE   0x40
#define RB_ENV_FLAG_LEVEL_RANGE  0x20
#define RB_ENV_FLAG_INIT_LEVEL   0x10
#define RB_ENV_FLAG_HIRES_TIME   0x08
#define RB_ENV_FLAG_HIRES_LEVEL  0x04
#define RB_ENV_FLAG_SIGNED_LEVEL 0x02
#define RB_ENV_FLAG_CURVE        0x01
// If PRESET:
#define RB_ENV_PRESET_ATTACK0  0x00 /* fast attack... */
#define RB_ENV_PRESET_ATTACK1  0x20
#define RB_ENV_PRESET_ATTACK2  0x40
#define RB_ENV_PRESET_ATTACK3  0x60 /* ...slow attack */
#define RB_ENV_PRESET_DECAY0   0x00 /* no decay... */
#define RB_ENV_PRESET_DECAY1   0x08
#define RB_ENV_PRESET_DECAY2   0x10
#define RB_ENV_PRESET_DECAY3   0x18 /* ...sharp decay */
#define RB_ENV_PRESET_RELEASE0 0x00 /* short release... */
#define RB_ENV_PRESET_RELEASE1 0x01
#define RB_ENV_PRESET_RELEASE2 0x02
#define RB_ENV_PRESET_RELEASE3 0x03
#define RB_ENV_PRESET_RELEASE4 0x04
#define RB_ENV_PRESET_RELEASE5 0x05
#define RB_ENV_PRESET_RELEASE6 0x06
#define RB_ENV_PRESET_RELEASE7 0x07 /* ...long release */

#endif
