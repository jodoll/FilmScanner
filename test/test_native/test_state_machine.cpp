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

StateMachine inCamera() {
  StateMachine machine;
  machine.bootCompleted();
  machine.shootCurrentFrame();
  return machine;
}

struct StartRejectionCase {
  const char* name;
  State expectedState;
  Mode expectedMode;
  Result (StateMachine::*event)();
  StateMachine (*makeMachine)();
};

class StartRejection : public ::testing::TestWithParam<StartRejectionCase> {};

TEST_P(StartRejection, ReturnsBusyAndLeavesStateUnchanged) {
  const StartRejectionCase testCase = GetParam();
  StateMachine machine = testCase.makeMachine();

  const Result result = (machine.*testCase.event)();

  ASSERT_EQ(result.error, ErrorCode::BUSY);
  EXPECT_EQ(result.state, testCase.expectedState);
  EXPECT_EQ(machine.state(), testCase.expectedState);
  EXPECT_EQ(machine.mode(), testCase.expectedMode);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

INSTANTIATE_TEST_SUITE_P(
    UnlistedStates, StartRejection,
    ::testing::Values(
        StartRejectionCase{"ContinuousRunFromBoot", State::BOOT, Mode::NONE,
                           &StateMachine::startContinuousRun, &inBoot},
        StartRejectionCase{"ContinuousRunFromSeek", State::SEEK, Mode::RUN,
                           &StateMachine::startContinuousRun, &inSeek},
        StartRejectionCase{"ContinuousRunFromJog", State::JOG, Mode::NONE,
                           &StateMachine::startContinuousRun, &inJog},
        StartRejectionCase{"ContinuousRunFromCamera", State::CAMERA, Mode::SHOOT,
                           &StateMachine::startContinuousRun, &inCamera},
        StartRejectionCase{"ScanOneFrameFromBoot", State::BOOT, Mode::NONE,
                           &StateMachine::scanOneFrame, &inBoot},
        StartRejectionCase{"ScanOneFrameFromSeek", State::SEEK, Mode::RUN,
                           &StateMachine::scanOneFrame, &inSeek},
        StartRejectionCase{"ScanOneFrameFromCamera", State::CAMERA, Mode::SHOOT,
                           &StateMachine::scanOneFrame, &inCamera},
        StartRejectionCase{"ScanOneFrameFromJog", State::JOG, Mode::NONE,
                           &StateMachine::scanOneFrame, &inJog},
        StartRejectionCase{"ShootCurrentFrameFromBoot", State::BOOT, Mode::NONE,
                           &StateMachine::shootCurrentFrame, &inBoot},
        StartRejectionCase{"ShootCurrentFrameFromSeek", State::SEEK, Mode::RUN,
                           &StateMachine::shootCurrentFrame, &inSeek},
        StartRejectionCase{"ShootCurrentFrameFromCamera", State::CAMERA,
                           Mode::SHOOT, &StateMachine::shootCurrentFrame,
                           &inCamera},
        StartRejectionCase{"ShootCurrentFrameFromJog", State::JOG, Mode::NONE,
                           &StateMachine::shootCurrentFrame, &inJog},
        StartRejectionCase{"JogFromBoot", State::BOOT, Mode::NONE,
                           &StateMachine::jog, &inBoot},
        StartRejectionCase{"JogFromSeek", State::SEEK, Mode::RUN,
                           &StateMachine::jog, &inSeek},
        StartRejectionCase{"JogFromCamera", State::CAMERA, Mode::SHOOT,
                           &StateMachine::jog, &inCamera},
        StartRejectionCase{"JogFromJog", State::JOG, Mode::NONE,
                           &StateMachine::jog, &inJog}),
    [](const ::testing::TestParamInfo<StartRejectionCase>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace scanner
