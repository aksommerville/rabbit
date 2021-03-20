/* rb_archive.h
 */
 
#ifndef RB_ARCHIVE_H
#define RB_ARCHIVE_H

#define RB_RES_TYPE(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|d)
#define RB_RES_TYPE_imag RB_RES_TYPE('i','m','a','g')
#define RB_RES_TYPE_song RB_RES_TYPE('s','o','n','g')
#define RB_RES_TYPE_snth RB_RES_TYPE('s','n','t','h')
#define RB_RES_TYPE_grid RB_RES_TYPE('g','r','i','d')
#define RB_RES_TYPE_text RB_RES_TYPE('t','e','x','t')
#define RB_RES_TYPE_data RB_RES_TYPE('d','a','t','a')

/* Opens, decompresses, and decodes an archive file.
 * Each member is presented to your callback in the order we find them.
 * Stops if you return nonzero, and returns the same.
 */
int rb_archive_read(
  const char *path,
  int (*cb_res)(uint32_t type,int id,const void *src,int srcc,void *userdata),
  void *userdata
);

#endif
