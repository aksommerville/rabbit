#include "test/rb_test.h"
#include "rabbit/rb_synth.h"
#include "rabbit/rb_synth_node.h"

RB_ITEST(expect_known_builtin_nodes) {

  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_noop))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_instrument))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_beep))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_gain))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_osc))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_env))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_add))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_mlt))
  RB_ASSERT(rb_synth_node_type_by_id(RB_SYNTH_NTID_fm))
  int expected_type_count=9;

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
