#include "rb_cli.h"
#include "rb_srcmon.h"
#include "rabbit/rb_fs.h"

/* Install encoded archive.
 */
 
static int rb_cli_synth_install_archive(
  struct rb_cli *cli,
  const void *src,int srcc,
  const char *path
) {
  if (rb_audio_lock(cli->audio)<0) return -1;
  if (rb_synth_configure(cli->synth,src,srcc)<0) {
    rb_audio_unlock(cli->audio);
    fprintf(stderr,"%s: Failed to install synth archive, %d bytes\n",path,srcc);
    return -1;
  }
  rb_audio_unlock(cli->audio);
  fprintf(stderr,"%s: Installed synth archive, %d bytes\n",path,srcc);
  return 0;
}

/* Compile and install instrument.
 */
 
static int rb_cli_synth_receive_instrument(
  struct rb_cli *cli,
  const char *path,
  int programid
) {
  fprintf(stderr,"%s:TODO: %s %d\n",path,__func__,programid);
  return 0;
}

/* Compile and install sound.
 */
 
static int rb_cli_synth_receive_sound(
  struct rb_cli *cli,
  const char *path,
  int id
) {
  fprintf(stderr,"%s:TODO: %s %d\n",path,__func__,id);
  return 0;
}

/* Install pre-encoded archive.
 */
 
static int rb_cli_synth_receive_archive(
  struct rb_cli *cli,
  const char *path
) {
  void *serial=0;
  int serialc=rb_file_read(&serial,path);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return 0;
  }
  int err=rb_cli_synth_install_archive(cli,serial,serialc,path);
  free(serial);
  return err;
}

/* File detected by srcmon.
 */
 
static int rb_cli_synth_cb_srcmon(
  const char *path,int pathc,
  const char *base,int basec,
  void *userdata
) {
  struct rb_cli *cli=userdata;
  int ftype=rb_ftype_from_path(path,pathc);
  switch (ftype) {
    case RB_FTYPE_INS: return rb_cli_synth_receive_instrument(cli,path,rb_id_from_basename(base,basec));
    case RB_FTYPE_SND: return rb_cli_synth_receive_sound(cli,path,rb_id_from_basename(base,basec));
    case RB_FTYPE_SYNTHAR: return rb_cli_synth_receive_archive(cli,path);
  }
  return 0;
}

/* Entry point.
 */
 
int rb_cli_main_synth(struct rb_cli *cli) {
  if (rb_cli_require_audio(cli)<0) return -1;
  if (rb_cli_require_srcmon(cli)<0) return -1;
  
  fprintf(stderr,"%s: Running. SIGINT to quit...\n",cli->exename);
  while (1) {
    if (rb_srcmon_update(cli->srcmon,rb_cli_synth_cb_srcmon,cli)<0) return -1;
    int err=rb_cli_update(cli);
    if (err<=0) return err;
  }
  
  return 0;
}
