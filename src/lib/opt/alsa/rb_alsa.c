#include "rabbit/rb_internal.h"
#include "rabbit/rb_audio.h"
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#define RB_ALSA_BUFFER_SIZE 2048

/* Instance
 */

struct rb_audio_alsa {
  struct rb_audio hdr;

  snd_pcm_t *alsa;
  snd_pcm_hw_params_t *hwparams;

  int hwbuffersize;
  int bufc; // frames
  int bufc_samples;
  int16_t *buf;

  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioabort;
  int cberror;
};

#define AUDIO ((struct rb_audio_alsa*)audio)

/* Cleanup.
 */

static void _rb_alsa_del(struct rb_audio *audio) {
  AUDIO->ioabort=1;
  if (AUDIO->iothd&&!AUDIO->cberror) {
    pthread_cancel(AUDIO->iothd);
    pthread_join(AUDIO->iothd,0);
  }
  pthread_mutex_destroy(&AUDIO->iomtx);
  if (AUDIO->hwparams) snd_pcm_hw_params_free(AUDIO->hwparams);
  if (AUDIO->alsa) snd_pcm_close(AUDIO->alsa);
  if (AUDIO->buf) free(AUDIO->buf);
}

/* I/O thread.
 */

static void *rb_alsa_iothd(void *dummy) {
  struct rb_audio *audio=dummy;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);//TODO copied from plundersquad -- is this correct?
  while (1) {
    pthread_testcancel();

    if (pthread_mutex_lock(&AUDIO->iomtx)) {
      AUDIO->cberror=1;
      return 0;
    }
    int err=audio->delegate.cb_pcm_out(AUDIO->buf,AUDIO->bufc_samples,audio);
    pthread_mutex_unlock(&AUDIO->iomtx);
    if (err<0) {
      AUDIO->cberror=1;
      return 0;
    }

    int16_t *samplev=AUDIO->buf;
    int samplep=0,samplec=AUDIO->bufc;
    while (samplep<samplec) {
      pthread_testcancel();
      err=snd_pcm_writei(AUDIO->alsa,samplev+samplep,samplec-samplep);
      if (AUDIO->ioabort) return 0;
      if (err<=0) {
        if ((err=snd_pcm_recover(AUDIO->alsa,err,0))<0) {
          AUDIO->cberror=1;
          return 0;
        }
        break;
      }
      samplep+=err;
    }
  }
  return 0;
}

/* Init.
 */

static int _rb_alsa_init(struct rb_audio *audio) {
  
  const char *device=audio->delegate.device;
  if (!device||!device[0]) device="default";
  if (
    (snd_pcm_open(&AUDIO->alsa,device,SND_PCM_STREAM_PLAYBACK,0)<0)||
    (snd_pcm_hw_params_malloc(&AUDIO->hwparams)<0)||
    (snd_pcm_hw_params_any(AUDIO->alsa,AUDIO->hwparams)<0)||
    (snd_pcm_hw_params_set_access(AUDIO->alsa,AUDIO->hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0)||
    (snd_pcm_hw_params_set_format(AUDIO->alsa,AUDIO->hwparams,SND_PCM_FORMAT_S16)<0)||
    (snd_pcm_hw_params_set_rate_near(AUDIO->alsa,AUDIO->hwparams,&audio->delegate.rate,0)<0)||
    (snd_pcm_hw_params_set_channels(AUDIO->alsa,AUDIO->hwparams,audio->delegate.chanc)<0)||
    (snd_pcm_hw_params_set_buffer_size(AUDIO->alsa,AUDIO->hwparams,RB_ALSA_BUFFER_SIZE)<0)||
    (snd_pcm_hw_params(AUDIO->alsa,AUDIO->hwparams)<0)
  ) return -1;

  if (snd_pcm_nonblock(AUDIO->alsa,0)<0) return -1;
  if (snd_pcm_prepare(AUDIO->alsa)<0) return -1;

  AUDIO->bufc=RB_ALSA_BUFFER_SIZE;
  AUDIO->bufc_samples=AUDIO->bufc*audio->delegate.chanc;
  if (!(AUDIO->buf=malloc(AUDIO->bufc_samples*sizeof(int16_t)))) return -1;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&AUDIO->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&AUDIO->iothd,0,rb_alsa_iothd,audio)) return -1;

  return 0;
}

/* Locks and maintenance.
 */

static int _rb_alsa_update(struct rb_audio *audio) {
  if (AUDIO->cberror) return -1;
  return 0;
}

static int _rb_alsa_lock(struct rb_audio *audio) {
  if (pthread_mutex_lock(&AUDIO->iomtx)) return -1;
  return 0;
}

static int _rb_alsa_unlock(struct rb_audio *audio) {
  pthread_mutex_unlock(&AUDIO->iomtx);
  return 0;
}

/* Type.
 */

static struct rb_audio_alsa _rb_alsa_singleton={0};

const struct rb_audio_type rb_audio_type_alsa={
  .name="alsa",
  .desc="PCM out via ALSA for Linux",
  .objlen=sizeof(struct rb_audio_alsa),
  .singleton=&_rb_alsa_singleton,
  .del=_rb_alsa_del,
  .init=_rb_alsa_init,
  .update=_rb_alsa_update,
  .lock=_rb_alsa_lock,
  .unlock=_rb_alsa_unlock,
};
