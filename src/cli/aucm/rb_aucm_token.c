#include "rb_aucm_internal.h"

/* Next token.
 */
 
int rb_aucm_next(struct rb_aucm *aucm) {
  
  // Advance past whatever we have.
  // No token may contain a newline, no need to check here.
  int srcp=aucm->token.v-aucm->src;
  if ((srcp<0)||(srcp>aucm->srcc)) return -1;
  srcp+=aucm->token.c;
  aucm->token.c=0;
  aucm->token.type=0;
  
  while (1) {
    const char *src=aucm->src+srcp;
    int srcc=aucm->srcc-srcp;
    aucm->token.v=src;
    
    // EOF
    if (srcc<=0) return 0;
    
    // Newline
    if (src[0]==0x0a) {
      aucm->token.lineno++;
      srcp++;
      continue;
    }
    
    // Whitespace
    if ((unsigned char)src[0]<=0x20) {
      srcp++;
      continue;
    }
    
    // Comments
    if (src[0]=='#') {
      int p=1;
      while ((p<srcc)&&(src[p]!=0x0a)) p++;
      srcp+=p;
      continue;
    }
    if ((srcc>=2)&&(src[0]=='/')) switch (src[1]) {
      case '/': {
          int p=2;
          while ((p<srcc)&&(src[p]!=0x0a)) p++;
          srcp+=p;
          continue;
        }
      case '*': {
          int p=2,nlc=0;
          while (1) {
            if (p>srcc-2) {
              aucm->token.c=2;
              return rb_aucm_error(aucm,"Unclosed block comment");
            }
            if ((src[p]=='*')&&(src[p+1]=='/')) {
              p+=2;
              break;
            }
            if (src[p]==0x0a) nlc++;
            p++;
          }
          aucm->token.lineno+=nlc;
          srcp+=p;
          continue;
        }
      case '{': {
          int p=2,nlc=0,depth=1;
          while (1) {
            if (p>srcc-2) {
              aucm->token.c=2;
              return rb_aucm_error(aucm,"Unclosed nesting comment");
            }
            if ((src[p]=='}')&&(src[p+1]=='/')) {
              p+=2;
              if (!--depth) break;
            } else if ((src[p]=='/')&&(src[p+1]=='{')) {
              p+=2;
              depth++;
            } else if (src[p]==0x0a) {
              nlc++;
              p++;
            } else {
              p++;
            }
          }
          aucm->token.lineno+=nlc;
          srcp+=p;
          continue;
        }
    }
    
    // Strings
    if (rb_aucm_isquote(src[0])) {
      int p=1;
      while (1) {
        if (p>=srcc) {
          aucm->token.c=1;
          return rb_aucm_error(aucm,"Unclosed string literal");
        }
        if (src[p]==src[0]) {
          p++;
          break;
        }
        if (src[p]=='\\') {
          if (++p>=srcc) {
            aucm->token.c=1;
            return rb_aucm_error(aucm,"Unclosed string literal");
          }
        }
        if ((src[p]<0x20)||(src[p]>0x7e)) {
          aucm->token.v=src+p;
          aucm->token.c=1;
          if (src[p]==0x0a) { // different message for newline, a common error
            aucm->token.v=src;
            aucm->token.c=p;
            return rb_aucm_error(aucm,"Unclosed string literal");
          } else {
            return rb_aucm_error(aucm,"Illegal byte 0x%02x in string literal",(unsigned char)(src[p]));
          }
        }
        p++;
      }
      aucm->token.c=p;
      aucm->token.type=RB_TOKEN_STRING;
      return 1;
    }
    
    // Numbers
    if (rb_aucm_isdigit(src[0])||(src[0]=='+')||(src[0]=='-')) {
      int p=1;
      while ((p<srcc)&&(rb_aucm_isident(src[p])||(src[p]=='.'))) p++;
      aucm->token.c=p;
      aucm->token.type=RB_TOKEN_NUMBER;
      return 1;
    }
    
    // Identifiers
    if (rb_aucm_isident(src[0])) {
      int p=1;
      while ((p<srcc)&&rb_aucm_isident(src[p])) p++;
      aucm->token.c=p;
      aucm->token.type=RB_TOKEN_IDENTIFIER;
      return 1;
    }
    
    // Invalid byte
    if ((src[0]<0x20)||(src[0]>0x7e)) {
      aucm->token.c=1;
      return rb_aucm_error(aucm,"Illegal byte 0x%02x",(unsigned char)(src[0]));
    }
    
    // Punctuation
    aucm->token.c=1;
    aucm->token.type=src[0];
    return 1;
  }
}
