#include "rb_cli.h"

/* File type from path.
 */
 
int rb_ftype_from_path(const char *path,int pathc) {
  if (!path) return RB_FTYPE_BINARY;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  
  // Find suffix. Everything after the last dot in the basename.
  const char *srcsfx=path+pathc;
  int srcsfxc=0;
  while (srcsfxc<pathc) {
    if (path[pathc-srcsfxc-1]=='/') {
      // No dot in basename, no suffix.
      srcsfxc=0;
      break;
    }
    if (path[pathc-srcsfxc-1]=='.') {
      break;
    }
    srcsfxc++;
    srcsfx--;
  }
  if (srcsfxc==pathc) {
    // No dot in basename, no suffix.
    srcsfxc=0;
  }
  
  // If we have a suffix of credible length, lowercase it and look for known ones.
  char sfx[8];
  if ((srcsfxc>0)&&(srcsfxc<=sizeof(sfx))) {
    memcpy(sfx,srcsfx,srcsfxc);
    int i=srcsfxc; while (i-->0) {
      if ((sfx[i]>='A')&&(sfx[i]<='Z')) sfx[i]+=0x20;
    }
    switch (srcsfxc) {
      case 3: {
          if (!memcmp(sfx,"txt",3)) return RB_FTYPE_TEXT;
          if (!memcmp(sfx,"mid",3)) return RB_FTYPE_MIDI;
          if (!memcmp(sfx,"ins",3)) return RB_FTYPE_INS;
          if (!memcmp(sfx,"snd",3)) return RB_FTYPE_SND;
          if (!memcmp(sfx,"png",3)) return RB_FTYPE_PNG;
          // Other things we are cognizant of, prevent further analysis:
          if (!memcmp(sfx,"gif",3)) return RB_FTYPE_OTHER;
          if (!memcmp(sfx,"jpg",3)) return RB_FTYPE_OTHER;
          if (!memcmp(sfx,"bmp",3)) return RB_FTYPE_OTHER;
        } break;
      case 4: {
          if (!memcmp(sfx,"jpeg",4)) return RB_FTYPE_OTHER;
        } break;
      case 5: {
          if (!memcmp(sfx,"synth",5)) return RB_FTYPE_SYNTHAR;
        } break;
    }
  }
  
  // No suffix or didn't match any known thing, consider the innermost directory name.
  const char *dir=0,*base=path;
  int dirc=0,basec=0,i=0;
  for (;i<pathc;i++) {
    if (path[i]=='/') {
      dir=base;
      dirc=basec;
      base=path+i+1;
      basec=0;
    } else {
      basec++;
    }
  }
  
  if ((dirc==5)&&!memcmp(dir,"image",5)) return RB_FTYPE_PNG;
  if ((dirc==4)&&!memcmp(dir,"song",4)) return RB_FTYPE_MIDI;
  if ((dirc==10)&&!memcmp(dir,"instrument",10)) return RB_FTYPE_INS;
  if ((dirc==5)&&!memcmp(dir,"sound",5)) return RB_FTYPE_SND;
  
  // "BINARY" means "OK really no idea".
  return RB_FTYPE_BINARY;
}

/* Resource ID from base name.
 */
 
int rb_id_from_basename(const char *base,int basec) {
  if (!base) return -1;
  if (basec<0) { basec=0; while (base[basec]) basec++; }
  int id=0,p=0;
  while (p<basec) {
    char ch=base[p];
    if ((ch<'0')||(ch>'9')) {
      // Require a clean token break: no letters. Underscore is ok.
      if ((ch>='a')&&(ch<='z')) return -1;
      if ((ch>='A')&&(ch<='Z')) return -1;
      break;
    }
    id*=10;
    id+=ch-'0';
    // Must be under a million (way covers all our use cases).
    // Check like this so we don't need to be mindful of overflow.
    if (id>=1000000) return -1;
    p++;
  }
  if (!p) return -1;
  return id;
}
