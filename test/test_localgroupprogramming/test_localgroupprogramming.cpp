#include "LocalGroupProgramming.h"
#include <unity.h>
using namespace LocalUi;
using namespace LocalUi::Advanced;
void setUp(){} void tearDown(){}
static void explicit_memory_then_broadcast_then_observation_then_metadata(){
  GroupProgrammingGuide guide;guide.begin(true,false);EntityAdmin::FormView view;
  guide.render(view);TEST_ASSERT_EQUAL(0,view.selected);TEST_ASSERT_FALSE(guide.pending());
  guide.turn(1);TEST_ASSERT_EQUAL(GroupProgrammingEffect::none,guide.activate());
  guide.render(view);TEST_ASSERT_EQUAL(0,view.selected);TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value,"40"));
  guide.turn(1);TEST_ASSERT_EQUAL(GroupProgrammingEffect::openMemory,guide.activate());
  TEST_ASSERT_TRUE(guide.pending());guide.complete(true);
  guide.render(view);TEST_ASSERT_EQUAL(0,view.selected);
  guide.turn(1);TEST_ASSERT_EQUAL(GroupProgrammingEffect::none,guide.activate());
  guide.render(view);TEST_ASSERT_NOT_NULL(strstr(view.hint,"Other motors"));TEST_ASSERT_EQUAL(0,view.selected);
  guide.turn(1);TEST_ASSERT_EQUAL(GroupProgrammingEffect::sendGroup,guide.activate());guide.complete(true);
  guide.render(view);TEST_ASSERT_NOT_NULL(strstr(view.hint,"add or remove"));TEST_ASSERT_EQUAL(0,view.selected);
  guide.turn(1);TEST_ASSERT_EQUAL(GroupProgrammingEffect::none,guide.activate());
  guide.render(view);TEST_ASSERT_EQUAL(0,view.selected);TEST_ASSERT_NOT_NULL(strstr(view.rows[2].label,"list only"));
  guide.turn(1);TEST_ASSERT_EQUAL(GroupProgrammingEffect::saveList,guide.activate());TEST_ASSERT_TRUE(guide.pending());
}
static void external_learn_mode_skips_only_memory_request(){
  GroupProgrammingGuide guide;guide.begin(false,true);guide.turn(-1);
  TEST_ASSERT_EQUAL(GroupProgrammingEffect::none,guide.activate());
  EntityAdmin::FormView view;guide.render(view);TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value,"1"));
  TEST_ASSERT_NOT_NULL(strstr(view.hint,"Other motors"));TEST_ASSERT_EQUAL(0,view.selected);
  guide.turn(1);TEST_ASSERT_EQUAL(GroupProgrammingEffect::sendGroup,guide.activate());guide.complete(true);
  guide.turn(1);guide.activate();guide.render(view);TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value,"Remove"));
}
static void refusal_cancellation_and_late_completion_cannot_advance(){
  GroupProgrammingGuide guide;guide.begin(true,false);TEST_ASSERT_EQUAL(GroupProgrammingEffect::close,guide.activate());
  guide.complete(true);TEST_ASSERT_EQUAL(GroupProgrammingEffect::none,guide.activate());
  guide.begin(true,false);guide.turn(1);guide.activate();guide.turn(1);guide.activate();
  guide.complete(false);guide.complete(true);EntityAdmin::FormView view;guide.render(view);
  TEST_ASSERT_EQUAL_STRING("Close",view.rows[0].label);TEST_ASSERT_EQUAL(GroupProgrammingEffect::close,guide.activate());
  guide.begin(true,true);guide.turn(1);guide.activate();guide.render(view);TEST_ASSERT_NOT_NULL(strstr(view.hint,"move"));
  guide.turn(1);guide.activate();TEST_ASSERT_EQUAL(GroupProgrammingEffect::close,guide.back());
  guide.complete(true);TEST_ASSERT_EQUAL(GroupProgrammingEffect::none,guide.activate());
}
int main(){UNITY_BEGIN();RUN_TEST(explicit_memory_then_broadcast_then_observation_then_metadata);
RUN_TEST(external_learn_mode_skips_only_memory_request);RUN_TEST(refusal_cancellation_and_late_completion_cannot_advance);return UNITY_END();}
