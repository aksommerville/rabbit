#ifndef RB_TEST_H
#define RB_TEST_H

#include "rabbit/rb_internal.h"

#define RB_ITEST(name,...) int name()
#define XXX_RB_ITEST(name,...) int name()

#define RB_UTEST(name,...) _RB_UTEST(1,name,#__VA_ARGS__)
#define XXX_RB_UTEST(name,...) _RB_UTEST(0,name,#__VA_ARGS__)

#define RB_ERROR_RETURN_VALUE -1

#define RB_FAIL(fmt,...) { \
  RB_FAIL_BEGIN(fmt,##__VA_ARGS__) \
  RB_FAIL_END \
}

#define RB_ASSERT(condition,...) if (!(condition)) { \
  RB_FAIL_BEGIN(""__VA_ARGS__) \
  RB_FAIL_MORE("Expected","true") \
  RB_FAIL_MORE("As written","%s",#condition) \
  RB_FAIL_END \
}

#define RB_ASSERT_NOT(condition,...) if (condition) { \
  RB_FAIL_BEGIN(""__VA_ARGS__) \
  RB_FAIL_MORE("Expected","false") \
  RB_FAIL_MORE("As written","%s",#condition) \
  RB_FAIL_END \
}

#define RB_ASSERT_INTS_OP(a,op,b,...) { \
  int _a=(int)(a),_b=(int)(b); \
  if (!(_a op _b)) { \
    RB_FAIL_BEGIN(""__VA_ARGS__) \
    RB_FAIL_MORE("As written","%s %s %s",#a,#op,#b) \
    RB_FAIL_MORE("Values","%d %s %d",_a,#op,_b) \
    RB_FAIL_END \
  } \
}

#define RB_ASSERT_INTS(a,b,...) RB_ASSERT_INTS_OP(a,==,b,##__VA_ARGS__)

#define RB_ASSERT_CALL(call,...) { \
  int _result=(call); \
  if (_result<0) { \
    RB_FAIL_BEGIN(""__VA_ARGS__) \
    RB_FAIL_MORE("Expected","successful call") \
    RB_FAIL_MORE("As written","%s",#call) \
    RB_FAIL_MORE("Result","%d",_result) \
    RB_FAIL_END \
  } \
}

#define RB_ASSERT_FAILURE(call,...) { \
  int _result=(call); \
  if (_result>=0) { \
    RB_FAIL_BEGIN(""__VA_ARGS__) \
    RB_FAIL_MORE("Expected","failed call") \
    RB_FAIL_MORE("As written","%s",#call) \
    RB_FAIL_MORE("Result","%d",_result) \
    RB_FAIL_END \
  } \
}

#define RB_ASSERT_STRINGS(a,ac,b,bc,...) { \
  const char *_a=(a),*_b=(b); \
  int _ac=(ac); if (!_a) _ac=0; else if (_ac<0) { _ac=0; while (_a[_ac]) _ac++; } \
  int _bc=(bc); if (!_b) _bc=0; else if (_bc<0) { _bc=0; while (_b[_bc]) _bc++; } \
  if ((_ac!=_bc)||memcmp(_a,_b,_ac)) { \
    RB_FAIL_BEGIN(""__VA_ARGS__) \
    RB_FAIL_MORE("(A) As written","%s : %s",#a,#ac) \
    RB_FAIL_MORE("(B) As written","%s : %s",#b,#bc) \
    if (rb_string_printable(_a,_ac)) RB_FAIL_MORE("(A) Value","'%.*s'",_ac,_a) \
    if (rb_string_printable(_b,_bc)) RB_FAIL_MORE("(B) Value","'%.*s'",_bc,_b) \
    RB_FAIL_END \
  } \
}

#define RB_ASSERT_FLOATS(a,b,e,...) { \
  double _a=(a),_b=(b),_e=(d); \
  double _d=_a-_b; \
  if (_d<0.0) _d=-_d; \
  if (_d>_e) { \
    RB_FAIL_BEGIN(""__VA_ARGS__) \
    RB_FAIL_MORE("As written","%s == %s within %s",#a,#b,#e) \
    RB_FAIL_MORE("Values","%f == %f within %f",_a,_b,_d) \
    RB_FAIL_MORE("Difference","%f",_d) \
    RB_FAIL_END \
  } \
}

/* Nothing for test cases below this point, just internals...
 ********************************************************************/
 
#define RB_FAIL_MORE(key,fmt,...) fprintf(stderr,"RB_TEST DETAIL | %16s: "fmt"\n",key,##__VA_ARGS__);

#define RB_FAIL_BEGIN(fmt,...) { \
  fprintf(stderr,"RB_TEST DETAIL +-------------------------------------------------------\n"); \
  fprintf(stderr,"RB_TEST DETAIL | Assertion failed!\n"); \
  RB_FAIL_MORE("Location","%s:%d",__FILE__,__LINE__) \
  if (fmt[0]) RB_FAIL_MORE("Message",fmt,##__VA_ARGS__) \
}

#define RB_FAIL_END { \
  fprintf(stderr,"RB_TEST DETAIL +-------------------------------------------------------\n"); \
  return RB_ERROR_RETURN_VALUE; \
}
 
#define _RB_UTEST(fallback,name,tags) { \
  if (rb_test_should_ignore(fallback,argc,argv,#name,rb_basename(__FILE__),tags)) { \
    fprintf(stderr,"RB_TEST SKIP %s [%s]\n",#name,rb_basename(__FILE__)); \
  } else if (name()<0) { \
    fprintf(stderr,"RB_TEST FAIL %s [%s]\n",#name,rb_basename(__FILE__)); \
  } else { \
    fprintf(stderr,"RB_TEST PASS %s [%s]\n",#name,rb_basename(__FILE__)); \
  } \
}

int rb_test_should_ignore(int fallback,int argc,char **argv,const char *name,const char *file,const char *tags);
const char *rb_basename(const char *path);
int rb_string_printable(const char *src,int srcc);

#endif
