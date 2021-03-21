#ifndef RB_EVDEV_INTERNAL_H
#define RB_EVDEV_INTERNAL_H

#include "rabbit/rb_internal.h"
#include "rabbit/rb_input.h"
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/poll.h>
#include <sys/inotify.h>
#include <linux/input.h>

struct rb_evdev_device {
  int devid;
  int fd; // Set <0 to drop device later. Disconnect fires at close, not at drop.
  int evdevid; // eg 3 for "/dev/input/event3"
};

struct rb_input_evdev {
  struct rb_input hdr;
  char *root; // "/dev/input/"
  int rootc;
  int fd;
  int refresh; // nonzero to scan at next update
  struct pollfd *pollfdv;
  int pollfda;
  struct rb_evdev_device *devicev;
  int devicec,devicea;
  int devid_next;
  char namestorage[64];
};

#define INPUT ((struct rb_input_evdev*)input)

void rb_evdev_device_cleanup(struct rb_evdev_device *device);

/* Set the root directory, initialize inotify, and set (refresh).
 * One can do this at any time, but normally just once, during init.
 */
int rb_evdev_set_root(struct rb_input *input,const char *path,int pathc);

int rb_evdev_drop_defunct_devices(struct rb_input *input);

/* Reallocate if needed, and populate (pollfdv) in preparation for poll().
 * Returns count of records.
 */
int rb_evdev_rebuild_pollfdv(struct rb_input *input);

/* Read from one file and take the appropriate actions.
 */
int rb_evdev_update_inotify(struct rb_input *input);
int rb_evdev_update_device(struct rb_input *input,int fd);

/* Read the root directory and try to connect anything that looks like an event device.
 */
int rb_evdev_scan(struct rb_input *input);

int rb_evdev_device_get_description(
  struct rb_input_device_description *desc,
  struct rb_evdev_device *device,
  struct rb_input *input
);

int rb_evdev_device_enumerate(
  struct rb_evdev_device *device,
  int (*cb)(int btnid,uint32_t hidusage,int value,int lo,int hi,void *userdata),
  void *userdata
);

struct rb_evdev_device *rb_evdev_device_by_fd(struct rb_input *input,int fd);
struct rb_evdev_device *rb_evdev_device_by_evdevid(struct rb_input *input,int evdevid);

int rb_evdev_add_device(struct rb_input *input,int fd,int evdevid);

uint32_t rb_evdev_guess_hidusage(uint8_t type,uint16_t code);

#endif
