#include <gtest/gtest.h>

#include "scanner_core/StateMachine.h"

namespace scanner {
namespace {

TEST(StateMachineBoot, StartsInBoot) {
  const StateMachine machine;

  EXPECT_EQ(machine.state(), State::BOOT);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_FALSE(machine.stopRequested());
  EXPECT_EQ(machine.lastError(), ErrorCode::NONE);
}

}  // namespace
}  // namespace scanner
