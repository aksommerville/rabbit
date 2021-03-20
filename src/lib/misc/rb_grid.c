#include "rabbit/rb_internal.h"
#include "rabbit/rb_grid.h"

/* New.
 */
 
struct rb_grid *rb_grid_new(int w,int h) {
  if ((w<1)||(w>RB_GRID_SIZE_LIMIT)) return 0;
  if ((h<1)||(h>RB_GRID_SIZE_LIMIT)) return 0;
  int objlen=sizeof(struct rb_grid)+w*h;
  struct rb_grid *grid=calloc(1,objlen);
  if (!grid) return 0;
  
  grid->refc=1;
  grid->w=w;
  grid->h=h;
  
  return grid;
}

/* Delete.
 */
 
void rb_grid_del(struct rb_grid *grid) {
  if (!grid) return;
  if (grid->refc-->1) return;
  free(grid);
}

/* Retain.
 */
 
int rb_grid_ref(struct rb_grid *grid) {
  if (!grid) return -1;
  if (grid->refc<1) return -1;
  if (grid->refc==INT_MAX) return -1;
  grid->refc++;
  return 0;
}
