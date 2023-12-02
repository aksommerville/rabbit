#include "rabbit/rb_internal.h"
#include "rabbit/rb_text.h"

/* Match pattern.
 */
 
static int rb_pattern_match_inner(
  const char *pat,int patc,
  const char *src,int srcc
) {
  int patp=0,srcp=0;
  while (1) {
  
    // Termination of (pat) is a mismatch, unless (src) is also terminated.
    if (patp>=patc) {
      if (srcp>=srcc) return 1;
      return 0;
    }
    
    // '*' matches any amount of anything, even when (src) exhausted.
    if (pat[patp]=='*') {
      patp++;
      // Consecutive stars are redundant.
      while ((patp<patc)&&(pat[patp]=='*')) patp++;
      // Star at the end is a guaranteed match.
      if (patp>=patc) return 1;
      // Skip as much as we need to.
      while (srcp<srcc) {
        if (rb_pattern_match_inner(pat+patp,patc-patp,src+srcp,srcc-srcp)) return 1;
        srcp++;
      }
      return 0;
    }
    
    // Termination of (src) is a mismatch.
    if (srcp>=srcc) return 0;
    
    // '\' forces a verbatim comparison.
    if (pat[patp]=='\\') {
      patp++;
      if (patp>=patc) return 0; // Invalid pattern, mismatch.
      if (pat[patp]!=src[srcp]) return 0;
      patp++;
      srcp++;
      continue;
    }
    
    // Whitespace collapses.
    if ((unsigned char)pat[patp]<=0x20) {
      if ((unsigned char)src[srcp]>0x20) return 0;
      patp++; while ((patp<patc)&&((unsigned char)pat[patp]<=0x20)) patp++;
      srcp++; while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
      continue;
    }
    
    // Force letters lower-case, and everything else is verbatim.
    char cha=pat[patp++]; if ((cha>='A')&&(cha<='Z')) cha+=0x20;
    char chb=src[srcp++]; if ((chb>='A')&&(chb<='Z')) chb+=0x20;
    if (cha!=chb) return 0;
  }
}
 
int rb_pattern_match(
  const char *pat,int patc,
  const char *src,int srcc
) {
  if (!pat) patc=0; else if (patc<0) { patc=0; while (pat[patc]) patc++; }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (patc&&((unsigned char)pat[patc-1]<=0x20)) patc--;
  while (patc&&((unsigned char)pat[0]<=0x20)) { pat++; patc--; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  return rb_pattern_match_inner(pat,patc,src,srcc);
}

/* Evaluate integer.
 */
 
int rb_int_eval(int *dst,const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int srcp=0,positive=1,base=10;
  
  if (srcp>=srcc) return -1;
  if (src[srcp]=='-') {
    if (++srcp>=srcc) return -1;
    positive=0;
  } else if (src[srcp]=='+') {
    if (++srcp>=srcc) return -1;
  }
  
  if ((srcp<srcc-2)&&(src[srcp]=='0')) switch (src[srcp+1]) {
    case 'x': case 'X': base=16; srcp+=2; break;
    case 'd': case 'D': base=10; srcp+=2; break;
    case 'o': case 'O': base=8; srcp+=2; break;
    case 'b': case 'B': base=2; srcp+=2; break;
  }
  
  if (srcp>=srcc) return -1;
  int limit=(positive?(UINT_MAX/base):(INT_MIN/base));
  *dst=0;
  while (srcp<srcc) {
    int digit=src[srcp++];
         if ((digit>='0')&&(digit<='9')) digit-='0';
    else if ((digit>='a')&&(digit<='f')) digit=digit-'a'+10;
    else if ((digit>='A')&&(digit<='F')) digit=digit-'A'+10;
    else return -1;
    if (digit>=base) return -1;
    if (positive) {
      if ((unsigned int)(*dst)>(unsigned int)limit) return -1;
      (*dst)*=base;
      if ((unsigned int)(*dst)>UINT_MAX-digit) return -1;
      (*dst)+=digit;
    } else {
      if (*dst<limit) return -1;
      (*dst)*=base;
      if (*dst<INT_MIN+digit) return -1;
      (*dst)-=digit;
    }
  }
  
  return 0;
}

/* Represent decimal signed integer.
 */
 
int rb_decsint_repr(char *dst,int dsta,int src) {
  if (src<0) {
    int limit=-10,dstc=2;
    while (src<=limit) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc;
    for (;i-->1;src/=10) dst[i]='0'-src%10;
    dst[0]='-';
    if (dstc<dsta) dst[dstc]=0;
    return dstc;
  } else {
    int limit=10,dstc=1;
    while (src>=limit) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc;
    for (;i-->0;src/=10) dst[i]='0'+src%10;
    if (dstc<dsta) dst[dstc]=0;
    return dstc;
  }
}
