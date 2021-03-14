#include "rb_cli.h"
#include "rb_ossmidi.h"

/* PCM callback.
 */
 
static int rb_cli_cb_pcm_out(int16_t *v,int c,struct rb_audio *audio) {
  struct rb_cli *cli=audio->delegate.userdata;
  if (cli->synth) {
    if (rb_synth_update(v,c,cli->synth)<0) return -1;
  } else {
    memset(v,0,c<<1);
  }
  return 0;
}

/* Initialize audio.
 */
 
int rb_cli_require_audio(struct rb_cli *cli) {

  if (!cli->audio) {
    const struct rb_audio_type *type;
    if (cli->audioname) {
      if (!(type=rb_audio_type_by_name(cli->audioname,-1))) {
        fprintf(stderr,"%s: Audio driver '%s' not found.\n",cli->exename,cli->audioname);
        return -1;
      }
    } else {
      if (!(type=rb_audio_type_by_index(0))) {
        fprintf(stderr,"%s: Failed to acquire default audio driver.\n",cli->exename);
        return -1;
      }
    }
    struct rb_audio_delegate delegate={
      .userdata=cli,
      .cb_pcm_out=rb_cli_cb_pcm_out,
      .rate=cli->audiorate,
      .chanc=cli->audiochanc,
    };
    if (!(cli->audio=rb_audio_new(type,&delegate))) {
      fprintf(stderr,
        "%s: Failed to instantiate audio driver '%s', rate=%d, chanc=%d\n",
        cli->exename,type->name,delegate.rate,delegate.chanc
      );
      return -1;
    }
  }
  
  if (!cli->synth) {
    if (!(cli->synth=rb_synth_new(cli->audio->delegate.rate,cli->audio->delegate.chanc))) {
      fprintf(stderr,"%s: Failed to create synthesizer.\n",cli->exename);
      return -1;
    }
  }
  
  if (!cli->ossmidi) {
    if (!(cli->ossmidi=rb_ossmidi_new("/dev/",-1))) {
      fprintf(stderr,"%s: Failed to initialize OSS for MIDI-In. Proceeding without.\n",cli->exename);
    }
  }
  
  return 0;
}
