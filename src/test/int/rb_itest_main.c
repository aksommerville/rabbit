#include "test/rb_test.h"
#include "test/int/rb_itest_toc.h"

int main(int argc,char **argv) {
  const struct rb_itest *test=rb_itestv;
  int i=sizeof(rb_itestv)/sizeof(struct rb_itest);
  for (;i-->0;test++) {
    if (rb_test_should_ignore(test->ignore,argc,argv,test->name,test->file,test->tags)) {
      fprintf(stderr,"RB_TEST SKIP %s [%s:%d]\n",test->name,test->file,test->line);
    } else if (test->fn()<0) {
      fprintf(stderr,"RB_TEST FAIL %s [%s:%d]\n",test->name,test->file,test->line);
    } else {
      fprintf(stderr,"RB_TEST PASS %s [%s:%d]\n",test->name,test->file,test->line);
    }
  }
  return 0;
}
