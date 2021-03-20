#include "rb_cli.h"

int main(int argc,char **argv) {
  struct rb_cli cli;
  if (rb_cli_init(&cli,argc,argv)<0) return 1;
  int err=0;
  switch (cli.command) {
  
    case RB_CLI_COMMAND_help: rb_cli_print_usage(&cli); break;
    case RB_CLI_COMMAND_synth: err=rb_cli_main_synth(&cli); break;
    case RB_CLI_COMMAND_plan: err=rb_cli_main_plan(&cli); break;
    case RB_CLI_COMMAND_archive: err=rb_cli_main_archive(&cli); break;
    case RB_CLI_COMMAND_synthc: err=rb_cli_main_synthc(&cli); break;
    case RB_CLI_COMMAND_imagec: err=rb_cli_main_imagec(&cli); break;
    
    default: rb_cli_print_usage(&cli); err=-1; break;
  }
  rb_cli_cleanup(&cli);
  if (err<0) return 1;
  return 0;
}
