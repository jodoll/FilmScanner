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

StateMachine inIdle() {
  StateMachine machine;
  machine.bootCompleted();
  return machine;
}

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

StateMachine inStopping() {
  StateMachine machine = inSeek();
  machine.notchDetected();
  return machine;
}

StateMachine inOffset() {
  StateMachine machine = inStopping();
  machine.motionCompleted();
  return machine;
}

StateMachine inCameraRun() {
  StateMachine machine = inOffset();
  machine.motionCompleted();
  return machine;
}

StateMachine inSingleSeek() {
  StateMachine machine;
  machine.bootCompleted();
  machine.scanOneFrame();
  return machine;
}

StateMachine inSingleStopping() {
  StateMachine machine = inSingleSeek();
  machine.notchDetected();
  return machine;
}

StateMachine inSingleOffset() {
  StateMachine machine = inSingleStopping();
  machine.motionCompleted();
  return machine;
}

StateMachine inSingleCamera() {
  StateMachine machine = inSingleOffset();
  machine.motionCompleted();
  return machine;
}

StateMachine inCameraSingle() {
  StateMachine machine = inSingleSeek();
  machine.notchDetected();
  machine.motionCompleted();
  machine.motionCompleted();
  return machine;
}

TEST(StateMachineSeek, NotchDetectedEntersStopping) {
  StateMachine machine = inSeek();

  const Result result = machine.notchDetected();

  ASSERT_EQ(result.state, State::STOPPING);
  EXPECT_EQ(machine.state(), State::STOPPING);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NOTCH));
}

TEST(StateMachineSeek, NotchTimeoutInRunEndsTheRun) {
  StateMachine machine = inSeek();

  const Result result = machine.notchTimedOut();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NOTCH_TIMEOUT);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::RUN_STOPPED));
}

TEST(StateMachineSeek, NotchTimeoutInSingleFailsTheStep) {
  StateMachine machine = inSingleSeek();

  const Result result = machine.notchTimedOut();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NOTCH_TIMEOUT);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::STEP_FAILED));
}

TEST(StateMachineSeek, SeekBudgetExhaustedInRunEndsTheRun) {
  StateMachine machine = inSeek();

  const Result result = machine.motionCompleted();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::MOTION_TIMEOUT);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::RUN_STOPPED));
}

TEST(StateMachineSeek, SeekBudgetExhaustedInSingleFailsTheStep) {
  StateMachine machine = inSingleSeek();

  const Result result = machine.motionCompleted();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::MOTION_TIMEOUT);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::STEP_FAILED));
}

TEST(StateMachineCamera, StopInShootStopsTheShot) {
  StateMachine machine = inCamera();

  const Result result = machine.stop();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::REQUEST);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::SHOT_STOPPED));
}

TEST(StateMachineJog, MotionCompletedEndsTheJog) {
  StateMachine machine = inJog();

  const Result result = machine.motionCompleted();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::JOG_DONE));
}

TEST(StateMachineJog, StopStopsTheJog) {
  StateMachine machine = inJog();

  const Result result = machine.stop();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::REQUEST);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::JOG_STOPPED));
}

TEST(StateMachineAbort, AbortInSeekEndsTheRun) {
  StateMachine machine = inSeek();

  const Result result = machine.abort();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_FALSE(machine.stopRequested());
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::ABORT);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::RUN_STOPPED));
}

StateMachine inFault() {
  StateMachine machine = inSeek();
  machine.driverFaultDetected();
  return machine;
}

struct ButtonCase {
  const char* name;
  Result (StateMachine::*event)();
  StateMachine (*makeMachine)();
};

class ButtonPress : public ::testing::TestWithParam<ButtonCase> {};

TEST_P(ButtonPress, StartsARunAndClearsTheError) {
  const ButtonCase testCase = GetParam();
  StateMachine machine = testCase.makeMachine();

  const Result result = (machine.*testCase.event)();

  ASSERT_EQ(result.state, State::SEEK);
  EXPECT_EQ(machine.state(), State::SEEK);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(machine.lastError(), ErrorCode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::RUN_STARTED));
}

INSTANTIATE_TEST_SUITE_P(
    FromIdleAndFault, ButtonPress,
    ::testing::Values(ButtonCase{"ShortFromIdle",
                                 &StateMachine::buttonPressedShort, &inIdle},
                      ButtonCase{"LongFromIdle",
                                 &StateMachine::buttonPressedLong, &inIdle},
                      ButtonCase{"ShortFromFault",
                                 &StateMachine::buttonPressedShort, &inFault},
                      ButtonCase{"LongFromFault",
                                 &StateMachine::buttonPressedLong, &inFault}),
    [](const ::testing::TestParamInfo<ButtonCase>& info) {
      return info.param.name;
    });

TEST(StateMachineBoot, ShortPressInBootIsRejected) {
  StateMachine machine;

  const Result result = machine.buttonPressedShort();

  ASSERT_EQ(result.error, ErrorCode::BUSY);
  EXPECT_EQ(machine.state(), State::BOOT);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineBoot, LongPressInBootIsRejected) {
  StateMachine machine;

  const Result result = machine.buttonPressedLong();

  ASSERT_EQ(result.error, ErrorCode::BUSY);
  EXPECT_EQ(machine.state(), State::BOOT);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineIdle, StopInIdleStaysIdle) {
  StateMachine machine = inIdle();

  const Result result = machine.stop();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineIdle, AbortInIdleStaysIdle) {
  StateMachine machine = inIdle();

  const Result result = machine.abort();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineFault, StartContinuousRunFromFaultStartsARunAndClearsTheError) {
  StateMachine machine = inFault();

  const Result result = machine.startContinuousRun();

  ASSERT_EQ(result.state, State::SEEK);
  EXPECT_EQ(machine.state(), State::SEEK);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(machine.lastError(), ErrorCode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::RUN_STARTED));
}

TEST(StateMachineFault, DriverFaultClearsALatchedStopRequest) {
  StateMachine machine = inSeek();
  machine.stop();

  machine.driverFaultDetected();

  EXPECT_EQ(machine.state(), State::FAULT);
  EXPECT_FALSE(machine.stopRequested());
}

TEST(StateMachineFault, DriverFaultInSeekEntersFault) {
  StateMachine machine = inSeek();

  const Result result = machine.driverFaultDetected();

  ASSERT_EQ(result.state, State::FAULT);
  EXPECT_EQ(machine.state(), State::FAULT);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(machine.lastError(), ErrorCode::DRIVER_FAULT);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::ERROR);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::RUN_STOPPED) |
                static_cast<uint16_t>(Announcement::FAULT_RAISED));
}

TEST(StateMachineFault, StopFromFaultReturnsToIdleAndClearsTheError) {
  StateMachine machine = inSeek();
  machine.driverFaultDetected();

  const Result result = machine.stop();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(machine.lastError(), ErrorCode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineFault, AbortFromFaultReturnsToIdleAndClearsTheError) {
  StateMachine machine = inSeek();
  machine.driverFaultDetected();

  const Result result = machine.abort();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(machine.lastError(), ErrorCode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineStopping, MotionCompletedEntersOffsetInRunMode) {
  StateMachine machine = inSeek();
  machine.notchDetected();

  const Result result = machine.motionCompleted();

  ASSERT_EQ(result.state, State::OFFSET);
  EXPECT_EQ(machine.state(), State::OFFSET);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

TEST(StateMachineOffset, MotionCompletedEntersCameraInRunMode) {
  StateMachine machine = inOffset();

  const Result result = machine.motionCompleted();

  ASSERT_EQ(result.state, State::CAMERA);
  EXPECT_EQ(machine.state(), State::CAMERA);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::FRAME_STARTED));
}

TEST(StateMachineCamera, PictureTakenAfterRunFrameEntersSeek) {
  StateMachine machine = inCameraRun();

  const Result result = machine.pictureTaken();

  ASSERT_EQ(result.state, State::SEEK);
  EXPECT_EQ(machine.state(), State::SEEK);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::FRAME_DONE));
}

TEST(StateMachineCamera, PictureTakenWithStopRequestedEndsTheRun) {
  StateMachine machine = inCameraRun();
  machine.stop();

  const Result result = machine.pictureTaken();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_FALSE(machine.stopRequested());
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::REQUEST);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::FRAME_DONE) |
                static_cast<uint16_t>(Announcement::RUN_STOPPED));
}

TEST(StateMachineCamera, PictureTakenInShootEndsTheShoot) {
  StateMachine machine = inCamera();

  const Result result = machine.pictureTaken();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::SHOT_DONE));
}

TEST(StateMachineCamera, PictureTakenInSingleEndsTheStep) {
  StateMachine machine = inCameraSingle();

  const Result result = machine.pictureTaken();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::FRAME_DONE) |
                static_cast<uint16_t>(Announcement::STEP_DONE));
}

struct RunFrameStopCase {
  const char* name;
  StateMachine (*makeMachine)();
};

class RunFrameStop : public ::testing::TestWithParam<RunFrameStopCase> {};

TEST_P(RunFrameStop, StopsAfterFrameAndStaysInState) {
  const RunFrameStopCase testCase = GetParam();
  StateMachine machine = testCase.makeMachine();
  const State expectedState = machine.state();

  const Result result = machine.stop();

  ASSERT_EQ(result.error, ErrorCode::NONE);
  EXPECT_TRUE(machine.stopRequested());
  EXPECT_EQ(machine.state(), expectedState);
  EXPECT_EQ(machine.mode(), Mode::RUN);
  EXPECT_EQ(result.state, expectedState);
  EXPECT_EQ(result.reason, StopReason::NONE);
  EXPECT_EQ(result.announcements, static_cast<uint16_t>(Announcement::NONE));
}

INSTANTIATE_TEST_SUITE_P(
    AllRunFrameStates, RunFrameStop,
    ::testing::Values(RunFrameStopCase{"Seek", &inSeek},
                      RunFrameStopCase{"Stopping", &inStopping},
                      RunFrameStopCase{"Offset", &inOffset},
                      RunFrameStopCase{"Camera", &inCameraRun}),
    [](const ::testing::TestParamInfo<RunFrameStopCase>& info) {
      return info.param.name;
    });

struct SingleFrameStopCase {
  const char* name;
  StateMachine (*makeMachine)();
};

class SingleFrameStop : public ::testing::TestWithParam<SingleFrameStopCase> {};

TEST_P(SingleFrameStop, StopsImmediatelyAndGoesIdle) {
  const SingleFrameStopCase testCase = GetParam();
  StateMachine machine = testCase.makeMachine();

  const Result result = machine.stop();

  ASSERT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_FALSE(machine.stopRequested());
  EXPECT_EQ(result.state, State::IDLE);
  EXPECT_EQ(result.reason, StopReason::REQUEST);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(Announcement::STEP_STOPPED));
}

INSTANTIATE_TEST_SUITE_P(
    AllSingleFrameStates, SingleFrameStop,
    ::testing::Values(SingleFrameStopCase{"Seek", &inSingleSeek},
                      SingleFrameStopCase{"Stopping", &inSingleStopping},
                      SingleFrameStopCase{"Offset", &inSingleOffset},
                      SingleFrameStopCase{"Camera", &inSingleCamera}),
    [](const ::testing::TestParamInfo<SingleFrameStopCase>& info) {
      return info.param.name;
    });

struct AbortCase {
  const char* name;
  Announcement stopAnnouncement;
  StateMachine (*makeMachine)();
};

class Abort : public ::testing::TestWithParam<AbortCase> {};

TEST_P(Abort, GoesIdleWithTheModeStopAnnouncement) {
  const AbortCase testCase = GetParam();
  StateMachine machine = testCase.makeMachine();

  const Result result = machine.abort();

  ASSERT_EQ(result.state, State::IDLE);
  EXPECT_EQ(machine.state(), State::IDLE);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_FALSE(machine.stopRequested());
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, StopReason::ABORT);
  EXPECT_EQ(result.announcements,
            static_cast<uint16_t>(testCase.stopAnnouncement));
}

INSTANTIATE_TEST_SUITE_P(
    EveryActiveState, Abort,
    ::testing::Values(AbortCase{"RunStopping", Announcement::RUN_STOPPED,
                                &inStopping},
                      AbortCase{"RunOffset", Announcement::RUN_STOPPED,
                                &inOffset},
                      AbortCase{"RunCamera", Announcement::RUN_STOPPED,
                                &inCameraRun},
                      AbortCase{"SingleSeek", Announcement::STEP_STOPPED,
                                &inSingleSeek},
                      AbortCase{"SingleStopping", Announcement::STEP_STOPPED,
                                &inSingleStopping},
                      AbortCase{"SingleOffset", Announcement::STEP_STOPPED,
                                &inSingleOffset},
                      AbortCase{"SingleCamera", Announcement::STEP_STOPPED,
                                &inSingleCamera},
                      AbortCase{"Shoot", Announcement::SHOT_STOPPED, &inCamera},
                      AbortCase{"Jog", Announcement::JOG_STOPPED, &inJog}),
    [](const ::testing::TestParamInfo<AbortCase>& info) {
      return info.param.name;
    });

struct FaultCase {
  const char* name;
  uint16_t announcements;
  StopReason reason;
  StateMachine (*makeMachine)();
};

class Fault : public ::testing::TestWithParam<FaultCase> {};

TEST_P(Fault, EntersFaultAndStoresDriverFault) {
  const FaultCase testCase = GetParam();
  StateMachine machine = testCase.makeMachine();

  const Result result = machine.driverFaultDetected();

  ASSERT_EQ(result.state, State::FAULT);
  EXPECT_EQ(machine.state(), State::FAULT);
  EXPECT_EQ(machine.mode(), Mode::NONE);
  EXPECT_EQ(machine.lastError(), ErrorCode::DRIVER_FAULT);
  EXPECT_EQ(result.error, ErrorCode::NONE);
  EXPECT_EQ(result.reason, testCase.reason);
  EXPECT_EQ(result.announcements, testCase.announcements);
}

const uint16_t kFaultRaised = static_cast<uint16_t>(Announcement::FAULT_RAISED);

INSTANTIATE_TEST_SUITE_P(
    EveryNonFaultState, Fault,
    ::testing::Values(
        FaultCase{"RunStopping",
                  static_cast<uint16_t>(Announcement::RUN_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inStopping},
        FaultCase{"RunOffset",
                  static_cast<uint16_t>(Announcement::RUN_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inOffset},
        FaultCase{"RunCamera",
                  static_cast<uint16_t>(Announcement::RUN_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inCameraRun},
        FaultCase{"SingleSeek",
                  static_cast<uint16_t>(Announcement::STEP_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inSingleSeek},
        FaultCase{"SingleStopping",
                  static_cast<uint16_t>(Announcement::STEP_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inSingleStopping},
        FaultCase{"SingleOffset",
                  static_cast<uint16_t>(Announcement::STEP_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inSingleOffset},
        FaultCase{"SingleCamera",
                  static_cast<uint16_t>(Announcement::STEP_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inSingleCamera},
        FaultCase{"Shoot",
                  static_cast<uint16_t>(Announcement::SHOT_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inCamera},
        FaultCase{"Jog",
                  static_cast<uint16_t>(Announcement::JOG_STOPPED) | kFaultRaised,
                  StopReason::ERROR, &inJog},
        FaultCase{"Idle", kFaultRaised, StopReason::NONE, &inIdle},
        FaultCase{"Boot", kFaultRaised, StopReason::NONE, &inBoot}),
    [](const ::testing::TestParamInfo<FaultCase>& info) {
      return info.param.name;
    });

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
