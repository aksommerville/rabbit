/* rb_fs.h
 * Helpers for reading and writing files.
 */
 
#ifndef RB_FS_H
#define RB_FS_H

/* Read a whole file in one shot.
 * This will only work for seekable (ie regular) files.
 * Allocates (*dstpp) on any success, even if zero length; you must free it.
 */
int rb_file_read(void *dstpp,const char *path);

/* Read a whole file in one shot without seeking.
 * This is more laborious than the regular read, but allows receiving streams from the OS.
 * We loop until the file reports EOF -- if that never happens, we block forever.
 */
int rb_file_read_pipesafe(void *dstpp,const char *path);

/* Write a whole file in one shot.
 * If it fails midway, we remove the file, sorry, it's irreversible.
 * (FWIW In the 21st century file I/O errors have become pretty rare...)
 */
int rb_file_write(const char *path,const void *src,int srcc);

/* Walk a directory and call cb_file for each immediate child.
 * Return nonzero to stop and return the same.
 * (type) will be present if dirent provides it, otherwise zero.
 */
int rb_dir_read(
  const char *path,
  int (*cb_file)(const char *path,int pathc,const char *base,int basec,char type,void *userdata),
  void *userdata
);

/* Get a file's type via stat.
 * This will follow symlinks, so we won't actually return 'l'.
 *   0: stat failed
 *   'f': regular file
 *   'd': directory
 *   'l': symlink (rb_dir_read only)
 *   's': socket
 *   '?': other
 */
char rb_file_get_type(const char *path);

/* (path) names a regular file.
 * We'll work backward to ensure its parent directory exist.
 */
int rb_mkdir_for_file(const char *path);

#endif
