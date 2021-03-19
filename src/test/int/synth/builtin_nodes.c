#include "test/rb_test.h"
#include "rabbit/rb_synth.h"
#include "rabbit/rb_synth_node.h"

RB_ITEST(expect_known_builtin_nodes) {

  int expected_type_count=0;
  #define _(tag) { \
    const struct rb_synth_node_type *type=rb_synth_node_type_by_id(RB_SYNTH_NTID_##tag); \
    RB_ASSERT(type,"%s",#tag) \
    RB_ASSERT_INTS(type->ntid,RB_SYNTH_NTID_##tag,"%s",#tag) \
    expected_type_count++; \
  }
  _(noop)
  _(instrument)
  _(beep)
  _(gain)
  _(osc)
  _(env)
  _(add)
  _(mlt)
  _(fm)
  _(multiplex)
  _(harm)
  #undef _

  int actual=0;
  for (;;actual++) {
    if (!rb_synth_node_type_by_index(actual)) break;
  }
  RB_ASSERT_INTS(actual,expected_type_count,"Update this test when you add or remove a node type")
  return 0;
}

// Every node type must pass rb_synth_node_type_validate()
// Mind that we don't do that at runtime for builtins; this test is important!
RB_ITEST(validate_builtin_nodes) {
  int p=0;
  for (;;p++) {
    const struct rb_synth_node_type *type=rb_synth_node_type_by_index(p);
    if (!type) break;
    RB_ASSERT_CALL(rb_synth_node_type_validate(type),"0x%02x %s",type->ntid,type->name)
  }
  return 0;
}
