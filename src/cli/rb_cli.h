#ifndef RB_CLI_H
#define RB_CLI_H

#include "rabbit/rb_internal.h"
#include "rabbit/rb_audio.h"
#include "rabbit/rb_synth.h"

struct rb_srcmon;
struct rb_ossmidi;

/* The "cli" object.
 *********************************************************/
 
#define RB_CLI_COMMAND_UNSET   0
#define RB_CLI_COMMAND_help    1
#define RB_CLI_COMMAND_synth   2
 
struct rb_cli {
// argv:
  const char *exename;
  int command;
  const char *audioname;
  int audiorate;
  int audiochanc;
  const char *datapath;
  const char **pargv;
  int pargc,parga;
// Global state by request only:
  struct rb_audio *audio;
  struct rb_synth *synth;
  struct rb_srcmon *srcmon;
  struct rb_ossmidi *ossmidi;
};

void rb_cli_cleanup(struct rb_cli *cli);

/* Blindly overwrites (cli).
 * (argv) must remain in scope, we may keep pointers into it.
 * We install signal handlers but never clean them up -- don't init more than once.
 */
int rb_cli_init(struct rb_cli *cli,int argc,char **argv);

void rb_cli_print_usage(struct rb_cli *cli);

/* Initializes (audio,synth,ossmidi) per (audioname,audiorate,audiochanc).
 * Defaults should work.
 */
int rb_cli_require_audio(struct rb_cli *cli);

/* Initializes (srcmon) per (datapath).
 * "--data" strongly recommended, tho it does have a default.
 * CLI common will create and delete the srcmon, but command implementation is responsible for polling it.
 */
int rb_cli_require_srcmon(struct rb_cli *cli);

/* Updates (audio,ossmidi).
 * Does not update (srcmon).
 * Sleeps for a brief interval.
 * Returns >0 to proceed, 0 to terminate (eg from signal), <0 for real errors.
 */
int rb_cli_update(struct rb_cli *cli);

/* Entry points per command.
 ************************************************************/
 
int rb_cli_main_synth(struct rb_cli *cli);

/* Helpers for serial data.
 ************************************************************/
 
#define RB_FTYPE_BINARY         0 /* Don't know specifically and it's binary, empty, or content not provided */
#define RB_FTYPE_TEXT           1 /* Don't know specifically, but looks like text */
#define RB_FTYPE_OTHER          2 /* Know what it is, and we're not interested */
#define RB_FTYPE_MIDI           3 /* Standard MIDI */
#define RB_FTYPE_INS            4 /* Text instrument (whole program) */
#define RB_FTYPE_SND            5 /* Text sound effect (combines into a multiplex program) */
#define RB_FTYPE_SYNTHAR        6 /* Binary synthesizer archive */
#define RB_FTYPE_PNG            7 /* Standard PNG */

// Mostly trusts the path's suffix, but may also examine leading directory names.
int rb_ftype_from_path(const char *path,int pathc);

// -1 or positive decimal integer from the start of (base), if there's a clean token break.
// We expect files named like "data/instrument/012-marimba.ins"
int rb_id_from_basename(const char *base,int basec);

void rb_dump_hex(const void *src,int srcc,const char *desc);

#endif
