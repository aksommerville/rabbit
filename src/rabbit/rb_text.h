/* rb_text.h
 */
 
#ifndef RB_TEXT_H
#define RB_TEXT_H

/* Nonzero if (src) matches pattern (pat).
 *  - ASCII letters are case-insensitive.
 *  - Leading and trailing space are ignored.
 *  - Inner whitespace collapses.
 *  - '*' in (pat) matches any amount of anything.
 *  - '\' in (pat) forces verbatim compare of one byte (eg case sensitive, escape wildcard)
 *  - No decoding: strings are compared bytewise.
 */
int rb_pattern_match(
  const char *pat,int patc,
  const char *src,int srcc
);

int rb_int_eval(int *dst,const char *src,int srcc);

int rb_decsint_repr(char *dst,int dsta,int src);

#endif
