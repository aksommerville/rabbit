#include "rabbit/rb_internal.h"
#include "rabbit/rb_image.h"

/* New.
 */
 
struct rb_image *rb_image_new(int w,int h) {
  if ((w<1)||(w>RB_IMAGE_SIZE_LIMIT)) return 0;
  if ((h<1)||(h>RB_IMAGE_SIZE_LIMIT)) return 0;
  int objlen=sizeof(struct rb_image)+4*w*h;
  struct rb_image *image=calloc(1,objlen);
  if (!image) return 0;
  image->refc=1;
  image->w=w;
  image->h=h;
  image->alphamode=RB_ALPHAMODE_BLEND;
  return image;
}

/* New framebuffer.
 */
 
struct rb_image *rb_framebuffer_new() {
  struct rb_image *image=rb_image_new(RB_FB_W,RB_FB_H);
  if (!image) return 0;
  image->alphamode=RB_ALPHAMODE_OPAQUE;
  return image;
}

/* Delete.
 */
 
void rb_image_del(struct rb_image *image) {
  if (!image) return;
  if (image->refc-->1) return;
  free(image);
}

/* Retain.
 */
 
int rb_image_ref(struct rb_image *image) {
  if (!image) return -1;
  if (image->refc<1) return -1;
  if (image->refc==INT_MAX) return -1;
  image->refc++;
  return 0;
}
