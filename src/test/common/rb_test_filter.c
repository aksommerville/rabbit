#include "test/rb_test.h"

/* String in list?
 */
 
static int rb_string_in_argv(const char *s,int sc,int argc,char **argv) {
  if (!s) sc=0; else if (sc<0) { sc=0; while (s[sc]) sc++; }
  int i=1; for (;i<argc;i++) {
    if (memcmp(argv[i],s,sc)) continue;
    if (argv[i][sc]) continue;
    return 1;
  }
  return 0;
}

/* Any from comma-delimited list in argv?
 */
 
static int rb_tags_in_argv(const char *tags,int argc,char **argv) {
  if (!tags) return 0;
  while (*tags) {
    if ((unsigned char)(*tags)<=0x20) { tags++; continue; }
    if (*tags==',') { tags++; continue; }
    const char *tag=tags;
    int tagc=0;
    while (*tags&&(*tags!=',')) { tags++; tagc++; }
    while (tagc&&((unsigned char)tag[tagc-1]<=0x20)) tagc--;
    if (rb_string_in_argv(tag,tagc,argc,argv)) return 1;
  }
  return 0;
}

/* Should ignore test?
 */
 
int rb_test_should_ignore(int fallback,int argc,char **argv,const char *name,const char *file,const char *tags) {
  if (argc<2) return fallback;
  if (rb_string_in_argv(name,-1,argc,argv)) return 0;
  if (rb_string_in_argv(file,-1,argc,argv)) return 0;
  if (rb_tags_in_argv(tags,argc,argv)) return 0;
  return 1;
}

/* Base name.
 */
 
const char *rb_basename(const char *path) {
  const char *base=path;
  for (;*path;path++) {
    if (*path=='/') base=path+1;
  }
  return base;
}

/* Is a string G0 only?
 */
 
int rb_string_printable(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc>128) return 0;
  for (;srcc-->0;src++) {
    if (*src<0x20) return 0;
    if (*src>0x7e) return 0;
  }
  return 1;
}
