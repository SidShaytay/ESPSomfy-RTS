#include <unity.h>
#include "LocalValueEditor.h"
#include <limits.h>
#include <math.h>
#include <string.h>
using namespace LocalUi;
void setUp() {}
void tearDown() {}
static void appendByte(ValueEditor &e,unsigned value,uint64_t now=1) {
  e.select(CharacterPicker::byteChoice,now); e.press(now);
  e.select(value,now); e.press(now);
}
static void original_text_and_cancel_never_mutate_caller() {
  char original[]="m\xc3\xa9t\xc3\xa9o";
  ValueEditor e;
  TEST_ASSERT_TRUE(e.beginText(original,20,TextEncoding::utf8,false,0));
  TEST_ASSERT_FALSE(e.dirty());
  TEST_ASSERT_EQUAL(int(EditOutcome::applied),int(e.apply(1)));
  char out[65]; TEST_ASSERT_TRUE(e.takeText(out,sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(original,out);
  TEST_ASSERT_FALSE(e.takeText(out,sizeof(out)));
  TEST_ASSERT_TRUE(e.beginText(original,20,TextEncoding::utf8,false,2));
  e.select(33,3); e.press(3); // A
  TEST_ASSERT_TRUE(e.dirty());
  e.cancel(); TEST_ASSERT_FALSE(e.takeText(out,sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("m\xc3\xa9t\xc3\xa9o",original);
}
static void exact_byte_input_validates_utf8_without_substitution() {
  ValueEditor e; char out[65];
  TEST_ASSERT_TRUE(e.beginText("",4,TextEncoding::utf8,false,0));
  appendByte(e,0xc3);
  TEST_ASSERT_EQUAL(int(EditOutcome::invalidUtf8),int(e.apply(2)));
  TEST_ASSERT_FALSE(e.takeText(out,sizeof(out)));
  appendByte(e,0xa9,3);
  TEST_ASSERT_EQUAL(int(EditOutcome::applied),int(e.apply(4)));
  TEST_ASSERT_TRUE(e.takeText(out,sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("\xc3\xa9",out);
  TEST_ASSERT_TRUE(e.beginText("\xc3\xa9",4,TextEncoding::utf8,false,0));
  char shortPreview[2]; e.preview(shortPreview,sizeof(shortPreview));
  TEST_ASSERT_EQUAL_STRING("",shortPreview); // never split a UTF-8 codepoint in preview
  e.cancel();
  const char *invalid[]={"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80"};
  for(auto text:invalid) {
    TEST_ASSERT_TRUE(e.beginText("",8,TextEncoding::utf8,false,0));
    for(size_t i=0;text[i];i++) appendByte(e,uint8_t(text[i]));
    TEST_ASSERT_EQUAL(int(EditOutcome::invalidUtf8),int(e.apply(2)));
    e.cancel();
  }
  TEST_ASSERT_TRUE(e.beginText("\xff",8,TextEncoding::utf8,false,0));
  TEST_ASSERT_EQUAL(int(EditOutcome::applied),int(e.apply(1))); // unchanged legacy bytes
  TEST_ASSERT_TRUE(e.takeText(out,sizeof(out))); TEST_ASSERT_EQUAL(255,uint8_t(out[0]));
  TEST_ASSERT_TRUE(e.beginText("",4,TextEncoding::bytes,false,0));
  appendByte(e,0xff);
  TEST_ASSERT_EQUAL(int(EditOutcome::applied),int(e.apply(2)));
  TEST_ASSERT_TRUE(e.takeText(out,sizeof(out))); TEST_ASSERT_EQUAL(255,uint8_t(out[0]));
}
static void text_bounds_and_explicit_apply_are_enforced() {
  char text[65]; memset(text,'x',63); text[63]=0;
  ValueEditor e; char out[65];
  TEST_ASSERT_TRUE(e.beginText(text,64,TextEncoding::utf8,false,0));
  e.select(33,1); e.press(1);
  TEST_ASSERT_EQUAL(64,e.length());
  e.press(2); TEST_ASSERT_EQUAL(64,e.length());
  TEST_ASSERT_EQUAL(int(EditOutcome::full),int(e.outcome()));
  TEST_ASSERT_FALSE(e.takeText(out,sizeof(out)));
  TEST_ASSERT_EQUAL(int(EditOutcome::applied),int(e.apply(3)));
  char small[4]="old";
  TEST_ASSERT_FALSE(e.takeText(small,sizeof(small))); // no silent truncation or consumption
  TEST_ASSERT_EQUAL_STRING("old",small);
  TEST_ASSERT_TRUE(e.takeText(out,sizeof(out)));
  TEST_ASSERT_EQUAL(64,strlen(out)); TEST_ASSERT_EQUAL('A',out[63]);
  TEST_ASSERT_FALSE(e.beginText(text,62,TextEncoding::utf8,false,0));
  TEST_ASSERT_FALSE(e.beginText("",65,TextEncoding::utf8,false,0));
  TEST_ASSERT_FALSE(e.beginText("",SIZE_MAX,TextEncoding::utf8,false,0));
  TEST_ASSERT_FALSE(e.beginText("",4,TextEncoding(99),false,0));
}
static void secret_preview_and_all_exit_paths_wipe_owned_buffers() {
  ValueEditor e;
  TEST_ASSERT_TRUE(e.beginText("synthetic-secret",64,TextEncoding::bytes,true,0));
  char preview[65]; e.preview(preview,sizeof(preview));
  TEST_ASSERT_EQUAL_STRING("****************",preview);
  e.cancel();
  const auto *raw=reinterpret_cast<const unsigned char *>(&e);
  for(size_t i=0;i+16<=sizeof(e);i++) TEST_ASSERT_NOT_EQUAL(0,memcmp(raw+i,"synthetic-secret",16));
  TEST_ASSERT_TRUE(e.beginText("another-secret",64,TextEncoding::bytes,true,10));
  e.tick(30009); TEST_ASSERT_TRUE(e.active());
  e.tick(30010); TEST_ASSERT_FALSE(e.active());
  TEST_ASSERT_EQUAL(int(EditOutcome::expired),int(e.outcome()));
  e.preview(preview,sizeof(preview)); TEST_ASSERT_EQUAL_STRING("",preview);
  TEST_ASSERT_TRUE(e.beginText("pending",64,TextEncoding::bytes,true,40000));
  e.apply(40001); e.tick(70001);
  char out[65]; TEST_ASSERT_FALSE(e.takeText(out,sizeof(out)));
  TEST_ASSERT_TRUE(e.beginText("rewind",64,TextEncoding::bytes,true,50000));
  e.tick(49999); TEST_ASSERT_EQUAL(int(EditOutcome::expired),int(e.outcome()));
  TEST_ASSERT_TRUE(e.beginText("poll-rewind",64,TextEncoding::bytes,true,100000));
  e.tick(110000); e.tick(109999);
  TEST_ASSERT_EQUAL(int(EditOutcome::expired),int(e.outcome()));
}
static void integer_drafts_saturate_without_signed_overflow() {
  ValueEditor e; int64_t out;
  TEST_ASSERT_TRUE(e.beginInteger(INT64_MAX-1,INT64_MIN,INT64_MAX,10,0));
  e.rotate(1,1); TEST_ASSERT_FALSE(e.takeNumber(out));
  e.apply(2); TEST_ASSERT_TRUE(e.takeNumber(out)); TEST_ASSERT_EQUAL_INT64(INT64_MAX,out);
  TEST_ASSERT_TRUE(e.beginInteger(INT64_MIN+1,INT64_MIN,INT64_MAX,10,0));
  e.rotate(-1,1); e.apply(2); e.takeNumber(out); TEST_ASSERT_EQUAL_INT64(INT64_MIN,out);
  TEST_ASSERT_TRUE(e.beginInteger(INT64_MIN,INT64_MIN,INT64_MAX,INT64_MAX,0));
  e.rotate(1,1); e.apply(2); e.takeNumber(out); TEST_ASSERT_EQUAL_INT64(-1,out);
  TEST_ASSERT_FALSE(e.beginInteger(1,2,10,1,0));
  TEST_ASSERT_FALSE(e.beginInteger(5,10,2,1,0));
  TEST_ASSERT_FALSE(e.beginInteger(5,0,10,0,0));
}
static void fixed_two_decimal_values_reject_nonfinite_and_bad_ranges() {
  ValueEditor e; int64_t cents;
  TEST_ASSERT_TRUE(e.beginFixed2(433.42,30000,92800,25,0));
  e.rotate(1,1); char preview[65]; e.preview(preview,sizeof(preview));
  TEST_ASSERT_EQUAL_STRING("433.67",preview);
  e.apply(2); TEST_ASSERT_TRUE(e.takeNumber(cents)); TEST_ASSERT_EQUAL_INT64(43367,cents);
  TEST_ASSERT_TRUE(e.beginFixed2(-0.01,-30,30,1,0));
  e.preview(preview,sizeof(preview)); TEST_ASSERT_EQUAL_STRING("-0.01",preview);
  TEST_ASSERT_TRUE(e.beginFixed2(433.419999999,30000,92800,1,0));
  e.apply(1); e.takeNumber(cents); TEST_ASSERT_EQUAL_INT64(43342,cents);
  TEST_ASSERT_FALSE(e.beginFixed2(NAN,0,100,1,0));
  TEST_ASSERT_FALSE(e.beginFixed2(INFINITY,0,100,1,0));
  TEST_ASSERT_FALSE(e.beginFixed2(1e300,INT64_MIN,INT64_MAX,1,0));
  TEST_ASSERT_FALSE(e.beginFixed2(999,30000,92800,1,0));
  TEST_ASSERT_FALSE(e.beginFixed2(92233720368547758.08,INT64_MIN,INT64_MAX,1,0));
}
static void boolean_and_choice_values_require_apply() {
  ValueEditor e; uint8_t out;
  TEST_ASSERT_TRUE(e.beginBoolean(false,0)); e.rotate(1,1);
  TEST_ASSERT_FALSE(e.takeChoice(out)); e.cancel(); TEST_ASSERT_FALSE(e.takeChoice(out));
  TEST_ASSERT_TRUE(e.beginBoolean(false,0)); e.rotate(1,1); e.apply(2);
  TEST_ASSERT_TRUE(e.takeChoice(out)); TEST_ASSERT_EQUAL(1,out);
  TEST_ASSERT_FALSE(e.beginChoice(0,0,0)); TEST_ASSERT_FALSE(e.beginChoice(3,3,0));
  TEST_ASSERT_TRUE(e.beginChoice(2,3,0)); e.rotate(1,1); e.apply(2); e.takeChoice(out);
  TEST_ASSERT_EQUAL(2,out); // list end never wraps
  TEST_ASSERT_TRUE(e.beginChoice(2,3,0)); e.rotate(-1,1); e.apply(2); e.takeChoice(out);
  TEST_ASSERT_EQUAL(1,out);
}
static void precision_change_never_applies_and_saturates_extreme_values() {
  ValueEditor e;int64_t out=0;
  TEST_ASSERT_FALSE(e.setStep(2,0));
  TEST_ASSERT_TRUE(e.beginInteger(1,INT64_MIN,INT64_MAX,1,0));
  TEST_ASSERT_FALSE(e.setStep(0,1));TEST_ASSERT_FALSE(e.setStep(-1,1));
  TEST_ASSERT_TRUE(e.setStep(INT64_MAX,1));TEST_ASSERT_FALSE(e.takeNumber(out));
  e.rotate(1,2);e.apply(3);TEST_ASSERT_TRUE(e.takeNumber(out));TEST_ASSERT_EQUAL_INT64(INT64_MAX,out);
  e.beginBoolean(false,4);TEST_ASSERT_FALSE(e.setStep(10,5));
}
static void long_text_preview_keeps_tail_and_masks_secrets() {
  ValueEditor e;char out[12];
  TEST_ASSERT_TRUE(e.beginText("0123456789TAIL",64,TextEncoding::bytes,false,0));
  e.previewTail(out,sizeof(out));TEST_ASSERT_EQUAL_STRING("...6789TAIL",out);
  TEST_ASSERT_TRUE(e.beginText("0123456789TAIL",64,TextEncoding::bytes,true,0));
  e.previewTail(out,sizeof(out));TEST_ASSERT_EQUAL_STRING("...********",out);TEST_ASSERT_EQUAL(14,e.length());
  TEST_ASSERT_TRUE(e.beginText("abcdef\xc3\xa9\xc3\xa9\xc3\xa9",64,TextEncoding::utf8,false,0));
  char small[9];e.previewTail(small,sizeof(small));TEST_ASSERT_EQUAL_STRING("...\xc3\xa9\xc3\xa9",small);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(long_text_preview_keeps_tail_and_masks_secrets);
  RUN_TEST(precision_change_never_applies_and_saturates_extreme_values);
  RUN_TEST(original_text_and_cancel_never_mutate_caller);
  RUN_TEST(exact_byte_input_validates_utf8_without_substitution);
  RUN_TEST(text_bounds_and_explicit_apply_are_enforced);
  RUN_TEST(secret_preview_and_all_exit_paths_wipe_owned_buffers);
  RUN_TEST(integer_drafts_saturate_without_signed_overflow);
  RUN_TEST(fixed_two_decimal_values_reject_nonfinite_and_bad_ranges);
  RUN_TEST(boolean_and_choice_values_require_apply);
  return UNITY_END();
}
