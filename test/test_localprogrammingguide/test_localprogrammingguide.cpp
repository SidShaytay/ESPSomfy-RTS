#include "LocalProgrammingGuide.h"
#include <unity.h>
#include <string.h>
#include <initializer_list>
using namespace LocalUi;
using namespace LocalUi::Advanced;
void setUp(){}void tearDown(){}
static void reviewed(ProgrammingGuide &guide,bool pair=true,bool oneButton=false){
  guide.begin(false,oneButton);
  guide.turn(1);if(!pair)guide.turn(1);
  TEST_ASSERT_EQUAL(ProgrammingEffect::none,guide.activate()); // intent, not RF
  guide.turn(1);TEST_ASSERT_EQUAL(ProgrammingEffect::none,guide.activate()); // operator opened learn mode
  guide.turn(1);guide.turn(1);TEST_ASSERT_EQUAL(ProgrammingEffect::none,guide.activate()); // review
}
static void opening_navigation_and_default_cancel_never_request_radio_or_marker(){
  ProgrammingGuide guide;guide.begin(false,false);
  TEST_ASSERT_EQUAL(ProgrammingEffect::close,guide.activate());
  reviewed(guide);EntityAdmin::FormView view;guide.render(view);
  TEST_ASSERT_EQUAL(0,view.selected);TEST_ASSERT_TRUE(view.rows[0].selected);
  TEST_ASSERT_EQUAL_STRING("Cancel",view.rows[0].label);TEST_ASSERT_TRUE(view.protectedFrame);
  TEST_ASSERT_EQUAL(ProgrammingEffect::none,guide.activate()); // back to draft
  TEST_ASSERT_FALSE(guide.pending());
  TEST_ASSERT_EQUAL(ProgrammingEffect::close,guide.back());
}
static void physical_response_and_separate_marker_confirmation_are_required(){
  for(bool pair:{false,true}){
    ProgrammingGuide guide;reviewed(guide,pair);guide.turn(1);
    TEST_ASSERT_EQUAL(ProgrammingEffect::enqueue,guide.activate());TEST_ASSERT_TRUE(guide.pending());
    guide.complete(true);TEST_ASSERT_FALSE(guide.pending());
    EntityAdmin::FormView view;guide.render(view);TEST_ASSERT_EQUAL(0,view.selected);
    TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value,"not confirmed"));
    TEST_ASSERT_NOT_NULL(strstr(view.hint,"add or remove"));
    guide.turn(1);TEST_ASSERT_EQUAL(ProgrammingEffect::none,guide.activate()); // human observed response
    guide.render(view);TEST_ASSERT_EQUAL(0,view.selected);TEST_ASSERT_EQUAL_STRING("Cancel",view.rows[0].label);
    guide.turn(1);TEST_ASSERT_EQUAL(pair?ProgrammingEffect::markPaired:ProgrammingEffect::markUnpaired,guide.activate());
    TEST_ASSERT_EQUAL(ProgrammingEffect::none,guide.activate()); // marker effect consumed once
  }
}
static void refusal_cancel_and_duplicate_completion_never_unlock_marker_save(){
  ProgrammingGuide guide;reviewed(guide);guide.turn(1);guide.activate();
  TEST_ASSERT_EQUAL(ProgrammingEffect::close,guide.back());guide.complete(true);
  TEST_ASSERT_EQUAL(ProgrammingEffect::none,guide.activate());
  reviewed(guide);guide.turn(1);guide.activate();guide.complete(false);guide.complete(true);
  EntityAdmin::FormView view;guide.render(view);TEST_ASSERT_EQUAL_STRING("Close",view.rows[0].label);
  TEST_ASSERT_EQUAL(ProgrammingEffect::close,guide.activate());
}
static void repeat_count_is_explicit_bounded_and_one_button_warning_is_visible(){
  ProgrammingGuide guide;guide.begin(true,true);guide.turn(1);guide.activate();guide.turn(1);guide.activate();
  guide.turn(1);guide.activate();guide.turn(-1);TEST_ASSERT_EQUAL(1,guide.repeats());
  for(unsigned i=0;i<200;++i)guide.turn(1);TEST_ASSERT_EQUAL(127,guide.repeats());
  guide.activate();guide.turn(1);guide.activate();EntityAdmin::FormView view;guide.render(view);
  TEST_ASSERT_NOT_NULL(strstr(view.rows[2].value,"127"));TEST_ASSERT_NOT_NULL(strstr(view.hint,"move"));
  TEST_ASSERT_EQUAL(0,view.selected);TEST_ASSERT_FALSE(guide.pending());
}
int main(){UNITY_BEGIN();RUN_TEST(opening_navigation_and_default_cancel_never_request_radio_or_marker);
 RUN_TEST(physical_response_and_separate_marker_confirmation_are_required);
 RUN_TEST(refusal_cancel_and_duplicate_completion_never_unlock_marker_save);
 RUN_TEST(repeat_count_is_explicit_bounded_and_one_button_warning_is_visible);return UNITY_END();}
