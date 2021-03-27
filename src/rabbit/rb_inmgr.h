/* rb_inmgr.h
 * High-level input manager.
 * Owns the driver relationships, handles mapping etc.
 */
 
#ifndef RB_INMGR_H
#define RB_INMGR_H

struct rb_inmgr;
struct rb_input;
struct rb_input_type;
struct rb_inmap;
struct rb_inmap_store;

/* (btnid) under 0x10000 are distinct bits; we will only report one at a time.
 * The current state of these buttons can be polled at any time.
 * Our model joystick matches SNES, 12 buttons.
 * 0x10000 and above, each (btnid) value is a unique button and state is not recorded.
 */
#define RB_BTNID_LEFT           0x00001 /* dpad... */
#define RB_BTNID_RIGHT          0x00002
#define RB_BTNID_UP             0x00004
#define RB_BTNID_DOWN           0x00008
#define RB_BTNID_A              0x00010 /* thumb buttons... bottom */
#define RB_BTNID_B              0x00020 /* left */
#define RB_BTNID_C              0x00040 /* right */
#define RB_BTNID_D              0x00080 /* top */
#define RB_BTNID_L              0x00100 /* left trigger */
#define RB_BTNID_R              0x00200 /* right trigger */
#define RB_BTNID_START          0x00400 /* principal aux */
#define RB_BTNID_SELECT         0x00800 /* secondary aux */
// 0x07000 reserved
#define RB_BTNID_CD             0x08000 /* carrier detect, nonzero if a device is connected */

#define RB_BTNID_HORZ (RB_BTNID_LEFT|RB_BTNID_RIGHT)
#define RB_BTNID_VERT (RB_BTNID_UP|RB_BTNID_DOWN)
#define RB_BTNID_DPAD (RB_BTNID_HORZ|RB_BTNID_VERT)

#define RB_PLAYER_LIMIT 8

struct rb_input_event {
  int plrid; // 0 if not mapped to a player
  int btnid; // 0 if raw device event only
  int value; // 0|1
  uint16_t state; // state for (plrid), including this event
  struct rb_input *source; // WEAK
  int devid;
  int devbtnid;
  int devvalue;
};

struct rb_inmgr_delegate {
  void *userdata;
  int (*cb_event)(struct rb_inmgr *inmgr,const struct rb_input_event *event);
};

struct rb_inmgr_player {
  int plrid;
  uint16_t state;
};

struct rb_inmgr {
  int refc;
  struct rb_input **inputv;
  int inputc,inputa;
  int playerc;
  int include_raw; // Set nonzero to forward all events, even those not mapped to a player
  struct rb_inmgr_delegate delegate;
  struct rb_inmgr_player playerv[1+RB_PLAYER_LIMIT];
  struct rb_inmap **inmapv;
  int inmapc,inmapa;
  struct rb_inmap_store *store;
};

struct rb_inmgr *rb_inmgr_new(const struct rb_inmgr_delegate *delegate);
void rb_inmgr_del(struct rb_inmgr *inmgr);
int rb_inmgr_ref(struct rb_inmgr *inmgr);

/* Initialize a new inmgr by instantiating input drivers.
 * Normally you'll want 'connect_all'.
 * If you want only specific types, use plain 'connect',
 */
int rb_inmgr_connect_all(struct rb_inmgr *inmgr);
int rb_inmgr_connect(struct rb_inmgr *inmgr,const struct rb_input_type *type);

/* Event callback will only fire during updates.
 */
int rb_inmgr_update(struct rb_inmgr *inmgr);

/* We assign devices to the requested players.
 * One player may have more than one device associated, or may have none.
 */
int rb_inmgr_set_player_count(struct rb_inmgr *inmgr,int playerc);

/* Current state of any player.
 * plrid zero is the aggregate of all players.
 */
uint16_t rb_inmgr_get_state(struct rb_inmgr *inmgr,int plrid);

int rb_inmgr_add_map(struct rb_inmgr *inmgr,struct rb_inmap *inmap);
int rb_inmgr_search_maps(struct rb_inmgr *inmgr,const struct rb_input *source,int devid);

int rb_input_button_repr(char *dst,int dsta,int btnid);
int rb_input_button_eval(const char *src,int srcc);

/* If the video manager provides a system keyboard, hook it up here.
 * Keycodes must be USB-HID page 7 -- Rabbit's own video drivers will always return in this format.
 */
int rb_inmgr_use_system_keyboard(struct rb_inmgr *inmgr);
int rb_inmgr_system_keyboard_event(struct rb_inmgr *inmgr,int keycode,int value);

#endif
