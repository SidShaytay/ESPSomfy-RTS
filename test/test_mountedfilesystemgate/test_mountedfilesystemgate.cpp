#include <unity.h>
#include "MountedFilesystemGate.h"
void setUp(){}void tearDown(){}
static void sealing_rejects_new_handles_and_waits_for_existing_close() {
  MountedFilesystemGate gate;
  TEST_ASSERT_FALSE(gate.drained());
  auto first=gate.enter();auto second=gate.enter();
  TEST_ASSERT_TRUE(bool(first));TEST_ASSERT_TRUE(bool(second));
  TEST_ASSERT_TRUE(gate.seal());TEST_ASSERT_FALSE(gate.seal());
  TEST_ASSERT_FALSE(bool(gate.enter()));TEST_ASSERT_FALSE(gate.drained());
  first.close();TEST_ASSERT_FALSE(gate.drained());
  second.close();TEST_ASSERT_TRUE(gate.drained());
  second.close();TEST_ASSERT_TRUE(gate.drained());
  gate.resume();TEST_ASSERT_FALSE(gate.drained());TEST_ASSERT_TRUE(bool(gate.enter()));
}
static void moved_handle_has_one_release_and_cancelled_seal_preserves_readers() {
  MountedFilesystemGate gate;auto original=gate.enter();auto moved=std::move(original);
  TEST_ASSERT_FALSE(bool(original));TEST_ASSERT_TRUE(bool(moved));
  gate.seal();original.close();TEST_ASSERT_FALSE(gate.drained());
  // Before unmount, an aborted preparation can resume without dropping readers.
  gate.resume();auto other=gate.enter();TEST_ASSERT_TRUE(bool(other));
  gate.seal();moved.close();TEST_ASSERT_FALSE(gate.drained());other.close();TEST_ASSERT_TRUE(gate.drained());
}
int main(){UNITY_BEGIN();RUN_TEST(sealing_rejects_new_handles_and_waits_for_existing_close);RUN_TEST(moved_handle_has_one_release_and_cancelled_seal_preserves_readers);return UNITY_END();}
