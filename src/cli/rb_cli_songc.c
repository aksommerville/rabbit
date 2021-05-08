#include "rb_cli.h"
#include "rb_png.h"
#include "rabbit/rb_fs.h"
#include "rabbit/rb_synth_event.h"

/* Convert in memory.
 */
 
static int rb_songc(void *dstpp,const char *src,int srcc,struct rb_cli *cli) {

  // All the real work happens right here:
  struct rb_song *song=rb_song_from_midi(src,srcc);
  if (!song) return -1;
  
  /* I think (ticksperqnote) can't be above 0x7fff, but (uspertick) might be able to overflow.
   * Whatever, it's easy to check.
   */
  if ((song->uspertick>0xffff)||(song->ticksperqnote>0xffff)) {
    fprintf(stderr,
      "MIDI timing not encodable: uspertick=%d, ticksperqnote=%d\n",
      song->uspertick,song->ticksperqnote
    );
    rb_song_del(song);
    return -1;
  }
  
  /* Easy to determine the output length: 16 byte header + 2 bytes per command.
   */
  int dstc=16+(song->cmdc<<1);
  uint8_t *dst=malloc(dstc);
  if (!dst) {
    rb_song_del(song);
    return -1;
  }
  
  /* Header.
   */
  memcpy(dst,"r\xabSg",4);
  dst[4]=song->uspertick>>8;
  dst[5]=song->uspertick;
  dst[6]=song->ticksperqnote>>8;
  dst[7]=song->ticksperqnote;
  memset(dst+8,0,8);
  
  /* Commands. Just force big-endian.
   */
  uint8_t *dstp=dst+16;
  const uint16_t *srcp=song->cmdv;
  int i=song->cmdc;
  for (;i-->0;dstp+=2,srcp++) {
    dstp[0]=(*srcp)>>8;
    dstp[1]=*srcp;
  }
  
  rb_song_del(song);
  *(void**)dstpp=dst;
  return dstc;
}

/* Main entry point.
 */
 
int rb_cli_main_songc(struct rb_cli *cli) {
  if (!cli->dstpath||!cli->dstpath[0]) {
    fprintf(stderr,"%s: '--dst=PATH' required\n",cli->exename);
    return -1;
  }
  
  void *src=0;
  int srcc=rb_file_read(&src,cli->datapath);
  if (srcc<0) {
    fprintf(stderr,"%s:ERROR: Failed to read file\n",cli->datapath);
    return -1;
  }
  
  void *dst=0;
  int dstc=rb_songc(&dst,src,srcc,cli);
  free(src);
  if (dstc<0) {
    fprintf(stderr,"%s:ERROR: Failed to convert song\n",cli->datapath);
    return -1;
  }
  
  if (rb_file_write(cli->dstpath,dst,dstc)<0) {
    fprintf(stderr,"%s:ERROR: Failed to write output (%d bytes)\n",cli->dstpath,dstc);
    free(dst);
    return -1;
  }
  free(dst);
  return 0;
}
