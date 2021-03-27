/* rb_inmap.h
 */
 
#ifndef RB_INMAP_H
#define RB_INMAP_H

struct rb_inmap;
struct rb_inmap_template;
struct rb_inmap_store;
struct rb_input_device_description;
struct rb_input;
struct rb_encoder;

/* Live map runner.
 *********************************************************/
 
struct rb_inmap {
  int plrid;
  struct rb_input *source; // WEAK
  int devid;
  uint16_t state;
  int refc;
  int (*cb)(struct rb_inmap *inmap,int btnid,int value);
  void *userdata;
  struct rb_inmap_field {
    int srcbtnid;
    int srcvalue;
    int srclo,srchi;
    int dstbtnid;
    int dstvalue;
  } *fieldv;
  int fieldc,fielda;
};

struct rb_inmap *rb_inmap_new();
void rb_inmap_del(struct rb_inmap *inmap);
int rb_inmap_ref(struct rb_inmap *inmap);

int rb_inmap_event(struct rb_inmap *inmap,int btnid,int value);

/* Returns a pointer to the first field for this srcbtnid -- there can be more than one.
 */
int rb_inmap_search(const struct rb_inmap *inmap,int srcbtnid);

struct rb_inmap_field *rb_inmap_insert(struct rb_inmap *inmap,int p,int srcbtnid);

/* Mask of every button <0x10000 that we think we can produce.
 */
uint16_t rb_inmap_get_mapped_buttons(const struct rb_inmap *inmap);

/* Template: Rules for matching a device and mapping its buttons.
 **********************************************************/
 
#define RB_INMAP_MODE_TWOSTATE    1 /* on if src in (lo..hi), typical buttons */
#define RB_INMAP_MODE_THREEWAY    2 /* analogue axis to two buttons and a dead zone. HORZ or VERT only */
#define RB_INMAP_MODE_YAWEERHT    3 /* threeway reversed */
#define RB_INMAP_MODE_HAT         4 /* single input with 8 angular values, and anything else means off. DPAD only */
 
struct rb_inmap_template {
  int refc;
  
  char *name; // case-insensitive, with wildcards. empty matches all
  int namec;
  uint16_t vid,pid; // zero matches all
  
  // Unlike runner, duplicate fields are NOT permitted in template.
  struct rb_inmap_template_field {
    int srcbtnid;
    int mode;
    int dstbtnid; // For THREEWAY or HAT, must contain multiple bits.
  } *fieldv;
  int fieldc,fielda;
};

struct rb_inmap_template *rb_inmap_template_new();
void rb_inmap_template_del(struct rb_inmap_template *tm);
int rb_inmap_template_ref(struct rb_inmap_template *tm);

int rb_inmap_template_match(
  const struct rb_inmap_template *tm,
  const struct rb_input_device_description *desc
);

/* Reads buttons existing on the device, may cause I/O.
 */
struct rb_inmap *rb_inmap_template_instantiate(
  struct rb_inmap_template *tm,
  struct rb_input *input,
  int devix
);

/* Create a new template by reading the device's capabilities and guessing where to map them.
 * (desc) is optional. If provided, we initialize matching criteria.
 * Optionally instantiates too on success (otherwise we have to enumerate buttons twice).
 */
struct rb_inmap_template *rb_inmap_template_synthesize(
  struct rb_input *input,
  int devix,
  const struct rb_input_device_description *desc,
  struct rb_inmap **instance
);

/* Encoding produces the block header and footer.
 * Decoding DOES NOT consume the header but DOES consume the footer.
 * Caller is expected to process the header.
 */
int rb_inmap_template_decode(struct rb_inmap_template *tm,const void *src,int srcc);
int rb_inmap_template_encode(struct rb_encoder *dst,const struct rb_inmap_template *tm);

int rb_inmap_template_search(const struct rb_inmap_template *tm,int srcbtnid);
int rb_inmap_template_add(struct rb_inmap_template *tm,int srcbtnid,int mode,int dstbtnid);

/* Store: Serializable collection of templates.
 ********************************************************/
 
struct rb_inmap_store {
  int refc;
  struct rb_inmap_template **tmv;
  int tmc,tma;
};

struct rb_inmap_store *rb_inmap_store_new();
void rb_inmap_store_del(struct rb_inmap_store *store);
int rb_inmap_store_ref(struct rb_inmap_store *store);

struct rb_inmap_template *rb_inmap_store_match_device(
  const struct rb_inmap_store *store,
  const struct rb_input_device_description *desc
);

int rb_inmap_store_add_template(
  struct rb_inmap_store *store,
  struct rb_inmap_template *tm
);

int rb_inmap_store_remove_template(
  struct rb_inmap_store *store,
  struct rb_inmap_template *tm
);

int rb_inmap_store_clear(struct rb_inmap_store *store);

int rb_inmap_store_decode(struct rb_inmap_store *store,const void *src,int srcc);
int rb_inmap_store_encode(struct rb_encoder *dst,const struct rb_inmap_store *store);

#endif
