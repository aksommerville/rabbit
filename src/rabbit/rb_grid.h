/* rb_grid.h
 * 2-dimensional array of cells, where each cell refers to a tile in some image.
 * Dimensions are fixed at construction and rows are always packed (stride==width).
 */
 
#ifndef RB_GRID_H
#define RB_GRID_H

#define RB_GRID_SIZE_LIMIT 1024

struct rb_grid {
  int refc;
  uint8_t imageid;
  int w,h; // in cells
  uint8_t v[];
};

struct rb_grid *rb_grid_new(int w,int h);
void rb_grid_del(struct rb_grid *grid);
int rb_grid_ref(struct rb_grid *grid);

//TODO serial

#endif
