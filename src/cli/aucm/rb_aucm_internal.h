#ifndef RB_AUCM_INTERNAL_H
#define RB_AUCM_INTERNAL_H

#include "cli/rb_cli.h"
#include "rb_aucm.h"
#include "rabbit/rb_synth_node.h"
#include "rabbit/rb_serial.h"

/* Token.
 ********************************************************/
 
struct rb_token {
  const char *v;
  int c;
  int type;
  int lineno;
};

#define RB_TOKEN_EOF 0
// Token type 0x21..0x7e are single-byte punctuation.
#define RB_TOKEN_NUMBER 0x80
#define RB_TOKEN_STRING 0x81
#define RB_TOKEN_IDENTIFIER 0x82

static inline int rb_aucm_isquote(char ch) {
  return ((ch=='"')||(ch=='\'')||(ch=='`'));
}
static inline int rb_aucm_isdigit(char ch) {
  return ((ch>='0')&&(ch<='9'));
}
static inline int rb_aucm_isident(char ch) {
  return (
    ((ch>='a')&&(ch<='z'))||
    ((ch>='A')&&(ch<='Z'))||
    ((ch>='0')&&(ch<='9'))||
    (ch=='_')
  );
}
static inline int rb_aucm_digit_eval(char ch) {
  if ((ch>='0')&&(ch<='9')) return ch-'0';
  if ((ch>='a')&&(ch<='z')) return ch-'a'+10;
  if ((ch>='A')&&(ch<='Z')) return ch-'A'+10;
  return -1;
}

/* Primitive evaluation.
 */
int rb_aucm_string_eval(char *dst,int dsta,const char *src,int srcc);
int rb_aucm_int_eval(int *dst,const char *src,int srcc);
int rb_aucm_float_eval(rb_sample_t *dst,const char *src,int srcc);

struct rb_scope {
  uint8_t programid;
  const struct rb_synth_node_type *type;
  const struct rb_synth_node_field *field;
  int serialfmt;
  uint8_t noteid;
};

/* Compiler.
 ********************************************************/

struct rb_aucm {
  struct rb_token token;
  const char *src;
  int srcc;
  const char *path;
  char *message;
  int messagec;
  int help; // If nonzero, we are failing due to "help" statement.
  struct rb_scope scope;
  struct rb_encoder bin;
};

void rb_aucm_cleanup(struct rb_aucm *aucm);

int rb_aucm_compile(void *dstpp,struct rb_aucm *aucm);

/* Set (message) if unset; and return -1.
 * Caller should arrange for (token) to be in a sensible place when returning.
 */
int rb_aucm_error(struct rb_aucm *aucm,const char *fmt,...);

/* Skip the current token and queue up the next one.
 * Returns >0 if another token is ready.
 * If this returns zero, (aucm->token.type) and (...c) are also zero, and if not, not.
 */
int rb_aucm_next(struct rb_aucm *aucm);

int rb_aucm_compile_internal(struct rb_aucm *aucm);
int rb_aucm_compile_statement(struct rb_aucm *aucm);
int rb_aucm_compile_block(struct rb_aucm *aucm);

/* Evaluate the current token.
 * Consumes the token on success, and generates a sensible error otherwise.
 */
const struct rb_synth_node_type *rb_aucm_eval_synth_node_type(struct rb_aucm *aucm);
const struct rb_synth_node_field *rb_aucm_eval_synth_node_field(struct rb_aucm *aucm,const struct rb_synth_node_type *type);
int rb_aucm_eval_int(int *v,struct rb_aucm *aucm,const char *desc,int lo,int hi);
int rb_aucm_eval_float(rb_sample_t *v,struct rb_aucm *aucm,const char *desc,rb_sample_t lo,rb_sample_t hi);

/* Dynamic help.
 **********************************************************/
 
// Nonzero if the current token is "help"
int rb_aucm_is_help(const struct rb_aucm *aucm);

int rb_aucm_help_program_node(struct rb_aucm *aucm);
int rb_aucm_help_inner_node(struct rb_aucm *aucm);
int rb_aucm_help_node_fields(struct rb_aucm *aucm,const struct rb_synth_node_type *type);
int rb_aucm_help_node_field(struct rb_aucm *aucm,const struct rb_synth_node_type *type,const struct rb_synth_node_field *field);
int rb_aucm_help_env(struct rb_aucm *aucm);
int rb_aucm_help_coefv(struct rb_aucm *aucm);
int rb_aucm_help_multiplex(struct rb_aucm *aucm);

#endif
