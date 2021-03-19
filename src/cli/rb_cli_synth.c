#include "rb_cli.h"
#include "rb_srcmon.h"
#include "aucm/rb_aucm.h"
#include "rabbit/rb_fs.h"
#include "rabbit/rb_program_store.h"
#include "rabbit/rb_synth_event.h"

/* Install encoded archive.
 */
 
static int rb_cli_synth_install_archive(
  struct rb_cli *cli,
  const void *src,int srcc,
  const char *path
) {
  //rb_dump_hex(src,srcc,path);
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

/* Switch to some program -- the last thing that changed.
 */
 
static int rb_cli_use_synth_program(
  struct rb_cli *cli,
  uint8_t programid,
  uint8_t noteid
) {
  if (rb_audio_lock(cli->audio)<0) return -1;
  int err=0,i=0;
  struct rb_synth_event event={
    .opcode=RB_SYNTH_EVENT_PROGRAM,
    .chid=0x00,
    .a=programid,
  };
  for (;i<16;i++) {
    event.chid=i;
    if (rb_synth_event(cli->synth,&event)<0) {
      err=-1;
      break;
    }
  }
  if ((noteid<0x80)&&!err) {
    event.opcode=RB_SYNTH_EVENT_NOTE_ON;
    event.chid=0x00;
    event.a=noteid;
    event.b=0x40;
    err=rb_synth_event(cli->synth,&event);
  }
  rb_audio_unlock(cli->audio);
  return err;
}

/* Compile and install instrument.
 */
 
static int rb_cli_synth_receive_instrument(
  struct rb_cli *cli,
  const char *path,
  int programid
) {
  if ((programid<0)||(programid>127)) {
    fprintf(stderr,"%s:WARNING: Ignoring instrument file, unable to determine programid.\n",path);
    return 0;
  }
  void *text=0;
  int textc=rb_file_read(&text,path);
  if (textc<0) {
    fprintf(stderr,"%s: Read failed\n",path);
    return 0;
  }
  void *bin=0;
  int binc=rb_instrument_compile(&bin,text,textc,programid,path);
  free(text);
  if (binc<0) {
    if (binc!=RB_CM_FULLY_LOGGED) {
      fprintf(stderr,"%s: Failed to compile instrument %d\n",path,programid);
    }
    return 0;
  }
  int err=rb_cli_synth_install_archive(cli,bin,binc,path);
  free(bin);
  if (err<0) return err;
  if (cli->srcmon->refresh) return 0;
  return rb_cli_use_synth_program(cli,programid,0xff);
}

/* Compile and install sound.
 */
 
static int rb_cli_synth_receive_sound(
  struct rb_cli *cli,
  const char *path,
  int id
) {
  int programid=-1,noteid=-1;
  if (id>=0) {
    programid=id/1000;
    noteid=id%1000;
  }
  if ((programid<0)||(programid>127)||(noteid<0)||(noteid>127)) {
    fprintf(stderr,"%s:WARNING: Ignoring sound file, unable to determine program or note id.\n",path);
    return 0;
  }
  const void *pgm=0;
  int pgmc=rb_program_store_get_serial(&pgm,cli->synth->program_store,programid);
  if (pgmc<0) pgmc=0;
  void *text=0;
  int textc=rb_file_read(&text,path);
  if (textc<0) {
    fprintf(stderr,"%s: Read failed\n",path);
    return 0;
  }
  void *bin=0;
  int binc=rb_sound_compile(&bin,text,textc,programid,noteid,pgm,pgmc,path);
  free(text);
  if (binc<0) {
    if (binc!=RB_CM_FULLY_LOGGED) {
      fprintf(stderr,"%s: Failed to compile sound %d\n",path,id);
    }
    return 0;
  }
  int err=rb_cli_synth_install_archive(cli,bin,binc,path);
  free(bin);
  if (err<0) return err;
  if (cli->srcmon->refresh) return 0;
  return rb_cli_use_synth_program(cli,programid,noteid);
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
