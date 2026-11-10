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

TEST(StateMachineStart, ContinuousRunEntersSeekInRunMode) {
  StateMachine machine;
  machine.bootCompleted();

  const Result result = machine.startContinuousRun();

  ASSERT_EQ(result.state, State::SEEK);
  EXPECT_EQ(machine.state(), State::SEEK);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::RUN_STARTED));
}

TEST(StateMachineStart, SingleFrameEntersSeekInSingleMode) {
  StateMachine machine;
  machine.bootCompleted();

  const Result result = machine.scanOneFrame();

  ASSERT_EQ(result.state, State::SEEK);
  EXPECT_EQ(machine.state(), State::SEEK);
  EXPECT_EQ(machine.mode(), Mode::SINGLE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineShoot, CurrentFrameEntersCameraInShootMode) {
  StateMachine machine;
  machine.bootCompleted();

  const Result result = machine.shootCurrentFrame();

  ASSERT_EQ(result.state, State::CAMERA);
  EXPECT_EQ(machine.state(), State::CAMERA);
  EXPECT_EQ(machine.mode(), Mode::SHOOT);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineJog, EntersJogWithoutMode) {
  StateMachine machine;
  machine.bootCompleted();

  const Result result = machine.jog();

  ASSERT_EQ(result.state, State::JOG);
  EXPECT_EQ(machine.state(), State::JOG);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

StateMachine inBoot() { return StateMachine{}; }

StateMachine inSeek() {
  StateMachine machine;
  machine.bootCompleted();
  machine.startContinuousRun();
  return machine;
}

StateMachine inJog() {
  StateMachine machine;
  machine.bootCompleted();
  machine.jog();
  return machine;
}

struct StartRejectionCase {
  State expectedState;
  Mode expectedMode;
  StateMachine (*makeMachine)();
};

class ContinuousRunRejection
    : public ::testing::TestWithParam<StartRejectionCase> {};

TEST_P(ContinuousRunRejection, ReturnsBusyAndLeavesStateUnchanged) {
  const StartRejectionCase testCase = GetParam();
  StateMachine machine = testCase.makeMachine();

  const Result result = machine.startContinuousRun();

  ASSERT_EQ(result.error, ErrorCode::BUSY);
  EXPECT_EQ(result.state, testCase.expectedState);
  EXPECT_EQ(machine.state(), testCase.expectedState);
  EXPECT_EQ(machine.mode(), testCase.expectedMode);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

INSTANTIATE_TEST_SUITE_P(
    UnlistedStates, ContinuousRunRejection,
    ::testing::Values(StartRejectionCase{State::BOOT, Mode::NONE, &inBoot},
                      StartRejectionCase{State::SEEK, Mode::RUN, &inSeek},
                      StartRejectionCase{State::JOG, Mode::NONE, &inJog}));

}  // namespace
}  // namespace scanner
