/* rb_input.h
 * Generic interface for input drivers.
 * There can be multiple drivers running at once.
 * Drivers describe their connected devices with a positive integer "devid".
 * To interact with a device more closely, you must acquire its transient "devix".
 * Devices have fields with an integer key and integer value, which we call "buttons".
 * (because they usually are buttons).
 *
 * Drivers are expected to operate on the main thread.
 * If an implementation uses a background thread for polling, 
 * it must report events only during rb_input_update().
 *
 * Keyboard and mouse input are generally delivered through the video driver, not input.
 *
 * Applications should interact with rb_inmgr, a higher-level abstraction.
 */
 
#ifndef RB_INPUT_H
#define RB_INPUT_H

struct rb_input;
struct rb_input_type;
struct rb_input_delegate;
struct rb_input_device_description;

/* Input driver instance.
 ****************************************************/
 
struct rb_input_delegate {
  void *userdata;
  
  /* Examine a device.
   * If you can't use it, it's wise to disconnect right away, to reduce noise.
   */
  int (*cb_connect)(struct rb_input *input,int devid);
  
  /* Notify that no more events will arrive for this device.
   * This is sent for driver-originated disconnection, and manual ones.
   * It is not called during shutdown.
   */
  int (*cb_disconnect)(struct rb_input *input,int devid);
  
  /* Some state on a device has changed.
   * (devid) was previously reported via connect, and not yet via disconnect.
   */
  int (*cb_event)(struct rb_input *input,int devid,int btnid,int value);
};

struct rb_input {
  const struct rb_input_type *type;
  int refc;
  struct rb_input_delegate delegate;
};

struct rb_input *rb_input_new(
  const struct rb_input_type *type,
  const struct rb_input_delegate *delegate
);

void rb_input_del(struct rb_input *input);
int rb_input_ref(struct rb_input *input);

int rb_input_update(struct rb_input *input);

/* (devix) for a device may change between events; (devid) must not.
 */
int rb_input_devix_from_devid(struct rb_input *input,int devid);
int rb_input_devid_from_devix(struct rb_input *input,int devix);

struct rb_input_device_description {
  uint16_t vid,pid,version; // Usually from USB.
  const char *name; // Copy if you want to keep it, may go out of scope fast.
  int namec;
};

/* Fetch IDs and name of a device.
 * Callers are advised to store this somewhere and avoid calling twice, there might be I/O involved.
 */
int rb_input_device_get_description(
  struct rb_input_device_description *desc,
  struct rb_input *input,
  int devix
);

/* Call (cb) for each button that this device can report.
 * The list is not guaranteed correct, in fact expect it to be a little off.
 * Clients should call this during connect to determine how to map the device.
 */
int rb_input_device_enumerate(
  struct rb_input *input,
  int devix,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
);

/* During the connect callback, you may decide you don't want a device, 
 * and it is wise to disconnect immediately.
 * ** Do not call this from your event callback **
 */
int rb_input_device_disconnect(struct rb_input *input,int devix);

/* Driver type.
 ********************************************************/
 
struct rb_input_type {
  const char *name;
  const char *desc;
  int objlen;
  void *singleton;
  
  void (*del)(struct rb_input *input);
  int (*init)(struct rb_input *input);
  
  // REQUIRED
  int (*update)(struct rb_input *input);
  
  // Optional: If both are null, devid and devix are the same thing.
  int (*devix_from_devid)(struct rb_input *input,int devid);
  int (*devid_from_devix)(struct rb_input *input,int devix);
  
  int (*device_get_description)(
    struct rb_input_device_description *desc,
    struct rb_input *input,
    int devix
  );
  
  int (*device_enumerate)(
    struct rb_input *input,
    int devix,
    int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
    void *userdata
  );
  
  int (*device_disconnect)(struct rb_input *input,int devix);
};

const struct rb_input_type *rb_input_type_by_name(const char *name,int namec);
const struct rb_input_type *rb_input_type_by_index(int p);

int rb_input_type_validate(const struct rb_input_type *type);

#endif
