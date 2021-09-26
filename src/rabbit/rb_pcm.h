/* rb_pcm.h
 */
 
#ifndef RB_PCM_H
#define RB_PCM_H

#include "rabbit/rb_signal.h"
#include <stdint.h>

struct rb_synth_node_config;
struct rb_synth_node_runner;

/* PCM dump and runner.
 ****************************************************************/

struct rb_pcm {
  int refc;
  int c;
  int16_t v[];
};

struct rb_pcm *rb_pcm_new(int c);
void rb_pcm_del(struct rb_pcm *pcm);
int rb_pcm_ref(struct rb_pcm *pcm);

struct rb_pcmrun {
  struct rb_pcm *pcm;
  int p;
};

// Blindly overwrites the runner.
int rb_pcmrun_init(struct rb_pcmrun *pcmrun,struct rb_pcm *pcm);

void rb_pcmrun_cleanup(struct rb_pcmrun *pcmrun);

/* Add to (v), mono only.
 * Returns >0 if more content remains, 0 if complete, never negative.
 */
int rb_pcmrun_update(int16_t *v,int c,struct rb_pcmrun *pcmrun);

/* PCM Printer.
 **************************************************************/

struct rb_pcmprint {
  struct rb_synth *synth; // WEAK
  int refc;
  struct rb_pcm *pcm;
  int p;
  struct rb_synth_node_runner *node;
  rb_sample_t *buf;
  int bufa;
  int16_t qlevel;
  uint16_t key;
};

struct rb_pcmprint *rb_pcmprint_new(
  struct rb_synth_node_config *node,
  uint8_t noteid
);

void rb_pcmprint_del(struct rb_pcmprint *pcmprint);
int rb_pcmprint_ref(struct rb_pcmprint *pcmprint);

// Generate at least (c) samples and return 0 if complete, >0 if more remain.
int rb_pcmprint_update(struct rb_pcmprint *pcmprint,int c);

#endif
