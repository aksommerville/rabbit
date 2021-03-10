/* rabbit.h
 */
 
#ifndef RABBIT_H
#define RABBIT_H

#include <stdint.h>

// Drivers.
struct rb_video;
struct rb_video_type;
struct rb_video_delegate;
struct rb_audio;
struct rb_audio_type;
struct rb_audio_delegate;

// Video.
struct rb_framebuffer;
struct rb_image;

// Audio.
struct rb_synth;
struct rb_song;
struct rb_song_player;
struct rb_synth_event;
struct rb_pcm;
struct rb_pcmprint;
struct rb_pcmrun;
struct rb_program_store;
struct rb_pcm_store;

#endif
