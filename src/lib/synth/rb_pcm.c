#include "rabbit/rb_internal.h"
#include "rabbit/rb_pcm.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"

/* PCM dump object.
 */
 
#define RB_PCM_SIZE_LIMIT 1000000 /* in samples regardless of main rate */

struct rb_pcm *rb_pcm_new(int c) {
  if (c<1) return 0;
  if (c>RB_PCM_SIZE_LIMIT) return 0;
  int objlen=sizeof(struct rb_pcm)+sizeof(int16_t)*c;
  struct rb_pcm *pcm=calloc(1,objlen);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=c;
  return pcm;
}
  
void rb_pcm_del(struct rb_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

int rb_pcm_ref(struct rb_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc==INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

/* PCM runner lifecycle.
 */

int rb_pcmrun_init(struct rb_pcmrun *pcmrun,struct rb_pcm *pcm) {
  if (rb_pcm_ref(pcm)<0) return -1;
  pcmrun->pcm=pcm;
  pcmrun->p=0;
  return 0;
}

void rb_pcmrun_cleanup(struct rb_pcmrun *pcmrun) {
  if (!pcmrun) return;
  rb_pcm_del(pcmrun->pcm);
  pcmrun->pcm=0;
  pcmrun->p=0;
}

/* Update runner.
 */
 
int rb_pcmrun_update(int16_t *v,int c,struct rb_pcmrun *pcmrun) {
  int cpc=pcmrun->pcm->c-pcmrun->p;
  if (cpc>c) cpc=c;
  if (cpc<1) return 0;
  const int16_t *src=pcmrun->pcm->v+pcmrun->p;
  pcmrun->p+=cpc;
  for (;cpc-->0;v++,src++) {
    (*v)+=(*src);
  }
  if (pcmrun->p>=pcmrun->pcm->c) return 0;
  return 1;
}

/* New printer.
 */

struct rb_pcmprint *rb_pcmprint_new(
  struct rb_synth_node_config *config,
  uint8_t noteid
) {
  if (!config) return 0;
  struct rb_pcmprint *pcmprint=calloc(1,sizeof(struct rb_pcmprint));
  if (!pcmprint) return 0;
  
  pcmprint->synth=config->synth;
  pcmprint->refc=1;
  pcmprint->qlevel=32000;
  
  // We need to supply an unchanging buffer to the node runner.
  // So we can't grow the buffer to suit update lengths.
  // Kind of of bummer.
  pcmprint->bufa=1024;
  if (!(pcmprint->buf=malloc(sizeof(rb_sample_t)*pcmprint->bufa))) {
    rb_pcmprint_del(pcmprint);
    return 0;
  }
  
  if (!(pcmprint->node=rb_synth_node_runner_new(config,&pcmprint->buf,1,noteid))) {
    rb_pcmprint_del(pcmprint);
    return 0;
  }
  
  int samplec=rb_synth_node_runner_get_duration(pcmprint->node);
  if (samplec<1) {
    rb_synth_error(config->synth,
      "'%s' node did not report a total duration; can't use as a main program.",
      config->type->name
    );
    rb_pcmprint_del(pcmprint);
    return 0;
  }
  if (!(pcmprint->pcm=rb_pcm_new(samplec))) {
    rb_pcmprint_del(pcmprint);
    return 0;
  }
  
  return pcmprint;
}

/* Printer lifecycle.
 */
 
void rb_pcmprint_del(struct rb_pcmprint *pcmprint) {
  if (!pcmprint) return;
  if (pcmprint->refc-->1) return;
  rb_synth_node_runner_del(pcmprint->node);
  rb_pcm_del(pcmprint->pcm);
  if (pcmprint->buf) free(pcmprint->buf);
  free(pcmprint);
}

int rb_pcmprint_ref(struct rb_pcmprint *pcmprint) {
  if (!pcmprint) return -1;
  if (pcmprint->refc<1) return -1;
  if (pcmprint->refc==INT_MAX) return -1;
  pcmprint->refc++;
  return 0;
}

/* Update printer.
 */
 
int rb_pcmprint_update(struct rb_pcmprint *pcmprint,int c) {
  if (!pcmprint->pcm) return 0;
  while (c>0) {
    int runc=pcmprint->pcm->c-pcmprint->p;
    if (runc<1) break;
    if (runc>c) runc=c;
    if (runc>pcmprint->bufa) runc=pcmprint->bufa;
    
    pcmprint->node->update(pcmprint->node,runc);
    
    rb_signal_quantize(pcmprint->pcm->v+pcmprint->p,pcmprint->buf,runc,pcmprint->qlevel);
    pcmprint->p+=runc;
    c-=runc;
  }
  if (pcmprint->p>=pcmprint->pcm->c) return 0;
  return 1;
}
