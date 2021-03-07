#include "rabbit/rb_internal.h"
#include "rabbit/rb_audio.h"
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <endian.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* Instance definition.
 */
 
typedef int16_t rb_sample_t;
 
struct rb_audio_pulse {
  struct rb_audio hdr;
  
  pa_simple *pa;
  
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int iocancel; // pa_simple doesn't like regular thread cancellation
  
  rb_sample_t *buf;
  int bufa;
};

#define AUDIO ((struct rb_audio_pulse*)audio)

/* I/O thread.
 */
 
static void *rb_pulse_iothd(void *arg) {
  struct rb_audio *audio=arg;
  while (1) {
    if (AUDIO->iocancel) return 0;
    
    // Fill buffer while holding the lock.
    if (pthread_mutex_lock(&AUDIO->iomtx)) {
      AUDIO->ioerror=-1;
      return 0;
    }
    if (!audio->delegate.cb_pcm_out) {
      pthread_mutex_unlock(&AUDIO->iomtx);
      usleep(10000);
      continue;
    }
    if (audio->delegate.cb_pcm_out(AUDIO->buf,AUDIO->bufa,audio)<0) {
      AUDIO->ioerror=-1;
      pthread_mutex_unlock(&AUDIO->iomtx);
      return 0;
    }
    if (pthread_mutex_unlock(&AUDIO->iomtx)) {
      AUDIO->ioerror=-1;
      return 0;
    }
    if (AUDIO->iocancel) return 0;
    
    // Deliver to PulseAudio.
    int err=0,result;
    result=pa_simple_write(AUDIO->pa,AUDIO->buf,sizeof(rb_sample_t)*AUDIO->bufa,&err);
    if (AUDIO->iocancel) return 0;
    if (result<0) {
      fprintf(stderr,"pa_simple_write: %s\n",pa_strerror(err));
      AUDIO->ioerror=-1;
      return 0;
    }
  }
}

/* Terminate I/O thread.
 */
 
static void rb_pulse_abort_io(struct rb_audio *audio) {
  if (!AUDIO->iothd) return;
  AUDIO->iocancel=1;
  pthread_join(AUDIO->iothd,0);
  AUDIO->iothd=0;
}

/* Cleanup.
 */
 
static void _rb_pulse_del(struct rb_audio *audio) {
  rb_pulse_abort_io(audio);
  pthread_mutex_destroy(&AUDIO->iomtx);
  if (AUDIO->pa) {
    pa_simple_free(AUDIO->pa);
  }
}

/* Init PulseAudio client.
 */
 
static int rb_pulse_init_pa(struct rb_audio *audio) {
  int err;

  pa_sample_spec sample_spec={
    #if BYTE_ORDER==BIG_ENDIAN
      .format=PA_SAMPLE_S16BE, // SAMPLETYPE
    #else
      .format=PA_SAMPLE_S16LE, // SAMPLETYPE
    #endif
    .rate=audio->delegate.rate,
    .channels=audio->delegate.chanc,
  };
  int bufframec=audio->delegate.rate/20; //TODO more sophisticated buffer length decision
  if (bufframec<20) bufframec=20;
  pa_buffer_attr buffer_attr={
    .maxlength=audio->delegate.chanc*sizeof(rb_sample_t)*bufframec,
    .tlength=audio->delegate.chanc*sizeof(rb_sample_t)*bufframec,
    .prebuf=0xffffffff,
    .minreq=0xffffffff,
  };
  
  if (!(AUDIO->pa=pa_simple_new(
    0, // server name
    "Beepbot", // our name
    PA_STREAM_PLAYBACK,
    0, // sink name (?)
    "Beepbot", // stream (as opposed to client) name
    &sample_spec,
    0, // channel map
    &buffer_attr,
    &err
  ))) {
    fprintf(stderr,"pa_simple_new: %s\n",pa_strerror(err));
    return -1;
  }
  
  audio->delegate.rate=sample_spec.rate;
  audio->delegate.chanc=sample_spec.channels;
  
  return 0;
}

/* With the final rate and channel count settled, calculate a good buffer size and allocate it.
 */
 
static int rb_pulse_init_buffer(struct rb_audio *audio) {

  const double buflen_target_s= 0.010; // about 100 Hz
  const int buflen_min=           128; // but in no case smaller than N samples
  const int buflen_max=         16384; // ...nor larger
  
  // Initial guess and clamp to the hard boundaries.
  AUDIO->bufa=buflen_target_s*audio->delegate.rate*audio->delegate.chanc;
  if (AUDIO->bufa<buflen_min) {
    AUDIO->bufa=buflen_min;
  } else if (AUDIO->bufa>buflen_max) {
    AUDIO->bufa=buflen_max;
  }
  // Reduce to next multiple of channel count.
  AUDIO->bufa-=AUDIO->bufa%audio->delegate.chanc;
  
  if (!(AUDIO->buf=malloc(sizeof(rb_sample_t)*AUDIO->bufa))) {
    return -1;
  }
  
  return 0;
}

/* Init thread.
 */
 
static int rb_pulse_init_thread(struct rb_audio *audio) {
  int err;
  
  if (err=pthread_mutex_init(&AUDIO->iomtx,0)) {
    fprintf(stderr,"pthread_mutex_init: %s\n",strerror(err));
    return -1;
  }

  if (err=pthread_create(&AUDIO->iothd,0,rb_pulse_iothd,audio)) {
    fprintf(stderr,"pthread_create: %s\n",strerror(err));
    return -1;
  }
  
  return 0;
}

/* Init.
 */
  
static int _rb_pulse_init(struct rb_audio *audio) {

  if (rb_pulse_init_pa(audio)<0) return -1;
  if (rb_pulse_init_buffer(audio)<0) return -1;
  if (rb_pulse_init_thread(audio)<0) return -1;
  
  return 0;
}

/* Update.
 */
  
static int _rb_pulse_update(struct rb_audio *audio) {
  if (AUDIO->ioerror) {
    AUDIO->ioerror=0;
    return -1;
  }
  return 0;
}

/* Lock.
 */
 
static int _rb_pulse_lock(struct rb_audio *audio) {
  if (pthread_mutex_lock(&AUDIO->iomtx)) return -1;
  return 0;
}

static int _rb_pulse_unlock(struct rb_audio *audio) {
  if (pthread_mutex_unlock(&AUDIO->iomtx)) return -1;
  return 0;
}

/* Type definition.
 */
 
static struct rb_audio_pulse _rb_pulse_singleton={0};
 
const struct rb_audio_type rb_audio_type_pulse={
  .name="pulse",
  .desc="PulseAudio for Linux",
  .objlen=sizeof(struct rb_audio_pulse),
  .singleton=&_rb_pulse_singleton,
  .del=_rb_pulse_del,
  .init=_rb_pulse_init,
  .update=_rb_pulse_update,
  .lock=_rb_pulse_lock,
  .unlock=_rb_pulse_unlock,
};
