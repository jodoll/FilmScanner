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

TEST(StateMachineBoot, MovesFromBootToIdle) {
  StateMachine machine;

  const Result result = machine.bootCompleted();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

}  // namespace
}  // namespace scanner
