#include "rabbit/rb_internal.h"
#include "rabbit/rb_image.h"
#include <math.h>
#include <endian.h>

/* Cleanup.
 */
 
void rb_lights_cleanup(struct rb_lights *lights) {
  if (lights->lightv) free(lights->lightv);
  rb_image_del(lights->scratch);
  memset(lights,0,sizeof(struct rb_lights));
}

/* Unused id.
 */
 
static int rb_lights_unused_id(const struct rb_lights *lights) {
  int max=0,i=lights->lightc;
  const struct rb_light *light=lights->lightv;
  for (;i-->0;light++) {
    if (light->id>max) max=light->id;
  }
  return max+1;
}

/* Add light.
 */

struct rb_light *rb_lights_add(struct rb_lights *lights,int id) {
  if (lights->lightc>=lights->lighta) {
    int na=lights->lighta+4;
    if (na>INT_MAX/sizeof(struct rb_light)) return 0;
    void *nv=realloc(lights->lightv,sizeof(struct rb_light)*na);
    if (!nv) return 0;
    lights->lightv=nv;
    lights->lighta=na;
  }
  struct rb_light *light=lights->lightv+lights->lightc++;
  memset(light,0,sizeof(struct rb_light));
  if (id>0) light->id=id;
  else light->id=rb_lights_unused_id(lights);
  return light;
}

/* Access to light list.
 */
 
struct rb_light *rb_lights_get(const struct rb_lights *lights,int id) {
  struct rb_light *light=lights->lightv;
  int i=lights->lightc;
  for (;i-->0;light++) {
    if (light->id==id) return light;
  }
  return 0;
}

int rb_lights_remove(struct rb_lights *lights,int id) {
  int i=lights->lightc;
  while (i-->0) {
    if (lights->lightv[i].id==id) {
      lights->lightc--;
      memmove(lights->lightv+i,lights->lightv+i+1,sizeof(struct rb_light)*(lights->lightc-i));
      return 0;
    }
  }
  return 0;
}

int rb_lights_clear(struct rb_lights *lights) {
  lights->lightc=0;
  return 0;
}

/* Darken one pixel.
 */
 
static inline uint32_t rb_lights_darken_pixel(uint32_t src,uint8_t brightness) {
  uint8_t *v=(uint8_t*)&src;
  #if BYTE_ORDER==BIG_ENDIAN
    v[1]=(v[1]*brightness)>>8;
    v[2]=(v[2]*brightness)>>8;
    v[3]=(v[3]*brightness)>>8;
  #else
    v[0]=(v[0]*brightness)>>8;
    v[1]=(v[1]*brightness)>>8;
    v[2]=(v[2]*brightness)>>8;
  #endif
  return src;
}

static inline void rb_lights_darken(
  uint32_t *v,int c,uint8_t brightness
) {
  for (;c-->0;v++) *v=rb_lights_darken_pixel(*v,brightness);
}

/* Draw.
 */
 
int rb_lights_draw(
  struct rb_image *dst,
  struct rb_lights *lights,
  int scrollx,int scrolly
) {
  if (!dst||(dst->alphamode!=RB_ALPHAMODE_OPAQUE)) return -1;

  // A background of 0xff is no-op: You can't see a light in a bright room.
  if (lights->bg==0xff) return 0;
  
  // Precalculate a few things for each light, and count how many are visible.
  struct rb_light *light=lights->lightv;
  int i=lights->lightc;
  int visiblec=0;
  for (;i-->0;light++) {
    // Invalid radii, not visible.
    if ((light->radius<0)||(light->gradius<0)||(!light->radius&&!light->gradius)) {
      light->visible=0;
      continue;
    }
    // Determine extents. Invisible if offscreen. Preserve the (y) ones, and the (x) ones we don't need.
    light->ya=light->y-scrolly;
    light->yz=light->ya+light->radius+light->gradius;
    light->ya-=light->radius+light->gradius;
    if ((light->yz<0)||(light->ya>=dst->h)) {
      light->visible=0;
      continue;
    }
    int xa=light->x-scrollx;
    int xz=xa+light->radius+light->gradius;
    xa-=light->radius+light->gradius;
    if ((xz<0)||(xa>=dst->w)) {
      light->visible=0;
      continue;
    }
    // Calculate the square radii.
    light->inner=light->radius*light->radius;
    if (light->gradius) {
      light->outer=(light->radius+light->gradius)*(light->radius+light->gradius);
      light->adjust=1.0/(light->outer-light->inner);
    } else {
      light->outer=light->inner;
      light->adjust=1.0; // irrelevant
    }
    light->visible=1;
    visiblec++;
  }
  
  // If no lights are visible, keep it simple.
  // This is a common case: Maybe using lights for overall fade-out.
  if (!visiblec) {
    rb_image_darken(dst,lights->bg);
    return 0;
  }
  
  // Walk rows and track one visible light that we know intersects.
  // (so we can avoid a lot of rechecking when there's a lot of lights).
  struct rb_light *currentlight=0;
  int pendingvacant=0;
  uint32_t *row=dst->pixels;
  int y=0; for (;y<dst->h;y++,row+=dst->w) {

    // Update (currentlight,pendingvacant), and handle vacant rows.
    if (currentlight) {
      if (y>currentlight->yz) {
        currentlight->visible=0;
        currentlight=0;
      }
    }
    if (!currentlight) {
      int ylo=dst->h;
      for (light=lights->lightv,i=lights->lightc;i-->0;light++) {
        if (!light->visible) continue;
        if (light->yz<y) {
          light->visible=0;
          continue;
        }
        if ((light->ya<=y)&&(light->yz>=y)) {
          currentlight=light;
          break;
        }
        if (light->ya<ylo) ylo=light->ya;
      }
      if (!currentlight) pendingvacant=ylo-y;
    }
    if (pendingvacant>0) {
      pendingvacant--;
      rb_lights_darken(row,dst->w,lights->bg);
      continue;
    }
  
    // Drawing a row with at least one light intersecting...
    uint32_t *p=row;
    int x=0; for (;x<dst->w;x++,p++) {
      uint8_t brightness=lights->bg;
      for (i=lights->lightc,light=lights->lightv;i-->0;light++) {
        if (!light->visible) continue;
        double dx=light->x-x,dy=light->y-y;
        double distance=dx*dx+dy*dy;
        if (distance<=light->inner) {
          brightness=0xff;
          break;
        }
        if (distance>=light->outer) continue;
        double norm=1.0-(distance-light->inner)*light->adjust;
        int inorm=brightness+(norm*(0xff-brightness));
        if (inorm<=brightness) continue;
        if (inorm>=0xff) {
          brightness=0xff;
          break;
        }
        brightness=inorm;
      }
      if (!brightness) *p=0;
      else if (brightness!=0xff) *p=rb_lights_darken_pixel(*p,brightness);
    }
  }

  return 0;
}
