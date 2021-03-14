#include "rb_cli.h"
#include "rb_srcmon.h"
#include "rb_ossmidi.h"
#include <unistd.h>
#include <signal.h>

/* Print usage.
 */
 
void rb_cli_print_usage(struct rb_cli *cli) {
  const struct rb_audio_type *default_audio=rb_audio_type_by_index(0);
  fprintf(stderr,
    "\n"
    "Usage: %s COMMAND [OPTIONS...]\n"
    "\n"
    "COMMAND:\n"
    "  help         Print this message.\n"
    "  synth        Initialize a synthesizer and watch input files.\n"
    "\n"
    "OPTIONS:\n"
    "  --audio=NAME    [%s] Audio driver.\n"
    "  --rate=HZ       [44100] Audio output rate.\n"
    "  --chanc=COUNT   [1] Audio channel count.\n"
    "  --data=PATH     [src/demo/data] Directory containing data input files.\n"
    "\n"
  ,cli->exename
  ,default_audio?default_audio->name:"ERROR!"
  );
}

/* Cleanup.
 */
 
void rb_cli_cleanup(struct rb_cli *cli) {
  if (cli->pargv) free(cli->pargv);
  
  rb_audio_del(cli->audio);
  rb_synth_del(cli->synth);
  rb_srcmon_del(cli->srcmon);
  rb_ossmidi_del(cli->ossmidi);
  
  memset(cli,0,sizeof(struct rb_cli));
}

/* Pre-init defaults.
 */
 
static void rb_cli_set_defaults(struct rb_cli *cli) {
  cli->exename="rabbit";
  cli->command=RB_CLI_COMMAND_UNSET;
  cli->audiorate=44100;
  cli->audiochanc=1;
  cli->datapath="src/demo/data";
}

/* Command.
 */
 
static int rb_cli_command_eval(const char *src) {
  while (*src=='-') src++;
  #define _(tag) if (!strcmp(src,#tag)) return RB_CLI_COMMAND_##tag;
  
  _(help)
  _(synth)
  
  #undef _
  return -1;
}

/* Receive option.
 */
 
static int rb_cli_receive_arg(struct rb_cli *cli,const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  #define STRARG(fldname,optname) { \
    if ((kc==sizeof(optname)-1)&&!memcmp(k,optname,kc)) { \
      cli->fldname=v; \
      return 0; \
    } \
  }
  #define INTARG(fldname,optname,lo,hi) { \
    if ((kc==sizeof(optname)-1)&&!memcmp(k,optname,kc)) { \
      int n=0,i=0; \
      for (;i<vc;i++) { \
        int digit=v[i]-'0'; \
        if ((digit<0)||(digit>9)) { \
          fprintf(stderr,"%s: Failed to parse '%.*s' as integer for '%.*s'\n",cli->exename,vc,v,kc,k); \
          return -1; \
        } \
        n*=10; \
        n+=digit; \
      } \
      if ((n<lo)||(n>hi)) { \
        fprintf(stderr,"%s: '%s' must be in %d..%d (have %d)\n",cli->exename,kc,k,lo,hi,n); \
        return -1; \
      } \
      cli->fldname=n; \
      return 0; \
    } \
  }
  
  STRARG(audioname,"audio")
  INTARG(audiorate,"rate",100,200000)
  INTARG(audiochanc,"chanc",1,8)
  STRARG(datapath,"data")
  
  #undef STRARG
  #undef INTARG
  if (vc) fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",cli->exename,kc,k,vc,v);
  else fprintf(stderr,"%s: Unexpected option '%.*s'\n",cli->exename,kc,k);
  return -1;
}

/* Add positional argument.
 */
 
static int rb_cli_pargv_append(struct rb_cli *cli,const char *src) {
  if (cli->pargc>=cli->parga) {
    int na=cli->parga+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(cli->pargv,sizeof(void*)*na);
    if (!nv) return -1;
    cli->pargv=nv;
    cli->parga=na;
  }
  cli->pargv[cli->pargc++]=src;
  return 0;
}

/* Signal handler and a loose global.
 */
 
static volatile int rb_cli_sigc=0;
 
static void rb_cli_rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++rb_cli_sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Initialize.
 */

int rb_cli_init(struct rb_cli *cli,int argc,char **argv) {
  memset(cli,0,sizeof(struct rb_cli));
  rb_cli_set_defaults(cli);
  
  signal(SIGINT,rb_cli_rcvsig);
  
  int argp=0;
  if (argp<argc) {
    cli->exename=argv[argp++];
  }
  
  if (argp<argc) {
    if ((cli->command=rb_cli_command_eval(argv[argp]))<0) {
      fprintf(stderr,"%s: Unknown command '%s'\n",cli->exename,argv[argp]);
      return -1;
    }
    argp++;
  }
  
  while (argp<argc) {
    const char *arg=argv[argp++];
    
    // Empty: Not allowed.
    if (!arg||!arg[0]) goto _unexpected_;
    
    // No dash: positional.
    if (arg[0]!='-') {
      if (rb_cli_pargv_append(cli,arg)<0) return -1;
      continue;
    }
    
    // Single dash: Not allowed.
    if (!arg[1]) goto _unexpected_;
    
    // Short options.
    if (arg[1]!='-') {
      const char *o=arg+1;
      for (;*o;o++) {
        if (rb_cli_receive_arg(cli,o,1,0,0)<0) return -1;
      }
      continue;
    }
    
    // More than two dashes, whatever, treat it like two.
    // Dashes followed by nothing: Illegal.
    while (*arg=='-') arg++;
    if (!*arg) goto _unexpected_;
    
    // Long options.
    const char *v=0;
    int kc=0;
    while (arg[kc]&&(arg[kc]!='=')) kc++;
    if (arg[kc]=='=') {
      v=arg+kc+1;
    } else if ((argp<argc)&&(argv[argp][0]!='-')) {
      v=argv[argp++];
    }
    if (rb_cli_receive_arg(cli,arg,kc,v,-1)<0) return -1;
    continue;
    
   _unexpected_:;
    fprintf(stderr,"%s: Unexpected argument '%s'\n",cli->exename,arg);
    return -1;
  }
  
  return 0;
}

/* Require source data monitor.
 */
 
int rb_cli_require_srcmon(struct rb_cli *cli) {
  if (cli->srcmon) return 0;
  if (!(cli->srcmon=rb_srcmon_new(cli->datapath,-1))) {
    fprintf(stderr,"%s: Failed to open source monitor on '%s'\n",cli->exename,cli->datapath);
    return -1;
  }
  fprintf(stderr,"%s: Monitoring source files.\n",cli->srcmon->root);
  return 0;
}

/* Receive serial MIDI data.
 */
 
static int rb_cli_cb_midi_in(const void *src,int srcc,const char *path,void *userdata) {
  struct rb_cli *cli=userdata;
  if (cli->audio) {
    if (rb_audio_lock(cli->audio)<0) return -1;
  }
  if (cli->synth) {
    if (rb_synth_events(cli->synth,src,srcc)<0) {
      fprintf(stderr,"%s: Failed to deliver %d bytes of MIDI to synthesizer\n",path,srcc);
    }
  } else {
    fprintf(stderr,"%s: Discarding %d bytes of MIDI; no synthesizer attached\n",path,srcc);
  }
  if (cli->audio) {
    rb_audio_unlock(cli->audio);
  }
  return 0;
}

/* Update.
 */
 
int rb_cli_update(struct rb_cli *cli) {
  if (rb_cli_sigc) {
    rb_cli_sigc=0;
    return 0;
  }
  if (cli->ossmidi) {
    if (rb_ossmidi_update(cli->ossmidi,rb_cli_cb_midi_in,cli)<0) return -1;
  }
  if (cli->audio) {
    if (rb_audio_update(cli->audio)<0) return -1;
  }
  usleep(10000);
  return 1;
}
