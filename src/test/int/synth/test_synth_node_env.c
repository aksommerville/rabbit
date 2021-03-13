#include "test/rb_test.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_synth.h"

static struct rb_synth mock_synth={
  .rate=300,
  .chanc=1,
};

static void wipe_mock_synth() {
  mock_synth.rate=300;
  mock_synth.chanc=1;
  mock_synth.message=0;
  mock_synth.messagec=0;
}

/* Measure a signal and print it to the console.
 */
 
static void print_signal(const rb_sample_t *v,int c) {
  int colc=159;
  int rowc=40;
  
  fprintf(stderr,"=== printing %d-sample signal ===\n",c);
  if (c>0) {
  
    rb_sample_t lo=v[0],hi=v[0];
    int i=c; while (i-->0) {
      if (v[i]<lo) lo=v[i];
      else if (v[i]>hi) hi=v[i];
    }
    rb_sample_t scale=(rowc-1)/(hi-lo);
    fprintf(stderr,"Range: %f .. %f\n",lo,hi);
    
    char *image=malloc(colc*rowc);
    if (image) {
      memset(image,0x20,colc*rowc);
      
      int x=0,pa=0;
      for (;x<colc;x++) {
        int pz=((x+1)*c)/colc;
        int pc=pz-pa;
        if (pc<1) pc=1;
        rb_sample_t plo=v[pa],phi=v[pa];
        for (i=pc;i-->0;) {
          if (v[pa+i]<plo) plo=v[pa+i];
          else if (v[pa+i]>phi) phi=v[pa+i];
        }
        pa=pz;
        
        int yhi=rowc-1-(plo-lo)*scale;
        int ylo=rowc-1-(phi-lo)*scale;
        int clip=0;
        if (ylo<0) { clip=1; ylo=0; }
        else if (ylo>=rowc) { clip=1; ylo=rowc-1; }
        if (yhi<ylo) yhi=ylo;
        else if (yhi>=rowc) { clip=1; yhi=rowc-1; }
        
        int y=ylo; for (;y<=yhi;y++) {
          image[y*colc+x]=clip?'!':'x';
        }
      }
      
      int y=0; for (;y<rowc;y++) {
        fprintf(stderr,"%.*s\n",colc,image+y*colc);
      }
      free(image);
    }
  }
  fprintf(stderr,"=== end of signal dump ===\n");
}

/* Instantiate a node config, run it to completion, and print the results to console.
 */
 
static int dump_signal(struct rb_synth_node_config *config) {
  #define buffera 1024
  rb_sample_t buffer[buffera];
  rb_sample_t *bufferv[]={buffer};
  struct rb_synth_node_runner *runner=rb_synth_node_runner_new(config,bufferv,1,0x40);
  RB_ASSERT(runner)
  
  int bufferc=rb_synth_node_runner_get_duration(runner);
  RB_ASSERT_INTS_OP(bufferc,>,0)
  RB_ASSERT_INTS_OP(bufferc,<=,buffera)
  
  runner->update(runner,bufferc);
  
  print_signal(buffer,bufferc);
  
  rb_synth_node_runner_del(runner);
  #undef buffera
  return 0;
}

/* Test case: Generate an envelope and dump it.
 */

RB_ITEST(generate_and_print_curve) {
  wipe_mock_synth();
  uint8_t serial[]={
    RB_SYNTH_NTID_env,
      0x01,0x00, // main
      0x02,RB_SYNTH_LINK_U8,0x01, // mode=set
      0x03,RB_SYNTH_LINK_U8,RB_ENV_FLAG_PRESET
        |RB_ENV_PRESET_RELEASE7
        ,
  };
  struct rb_synth_node_config *config=rb_synth_node_config_new_decode(&mock_synth,serial,sizeof(serial));
  if (mock_synth.messagec) fprintf(stderr,"%.*s\n",mock_synth.messagec,mock_synth.message);
  RB_ASSERT(config)
  
  RB_ASSERT_CALL(dump_signal(config))
  
  rb_synth_node_config_del(config);
  return 0;
}
